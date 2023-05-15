#pragma once
/*#
    # ui_game.h

    Integrated debugging UI for game.h

    Do this:
    ~~~C
    #define GAME_UI_IMPL
    ~~~
    before you include this file in *one* C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation

    ~~~C
    GAME_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    ## zlib/libpng license

    Copyright (c) 2023 Scemino
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x, y;
    int w, h;
    bool open;
    ui_dbg_texture_callbacks_t texture_cbs;
    void* tex_fb[4];
    uint32_t pixel_buffer[GAME_WIDTH*GAME_HEIGHT];
} ui_game_video_t;

typedef struct {
    int x, y;
    int w, h;
    bool open;
    void* tex_bmp;
    uint8_t buf[GAME_WIDTH*GAME_HEIGHT*4];
    int selected;
    struct {
        int pal_idx;
        int pal_res_idx;
        bool pal_ovw;
    } data;
    bool filters[7];
} ui_game_res_t;

typedef struct {
    int x, y;
    int w, h;
    bool open;
} ui_game_vm_t;

typedef struct {
    game_t* game;
    ui_dbg_texture_callbacks_t dbg_texture;     // debug texture create/update/destroy callbacks
    ui_dbg_keys_desc_t dbg_keys;                // user-defined hotkeys for ui_dbg_t
    ui_snapshot_desc_t snapshot;                // snapshot ui setup params
} ui_game_desc_t;

typedef struct {
    game_t* game;
    ui_game_video_t video;
    ui_game_res_t res;
    ui_game_vm_t vm;
    ui_dbg_t dbg;
    ui_snapshot_t snapshot;
    ui_dasm_t dasm[4];
} ui_game_t;

void ui_game_init(ui_game_t* ui, const ui_game_desc_t* desc);
void ui_game_discard(ui_game_t* ui);
void ui_game_draw(ui_game_t* ui);
game_debug_t ui_game_get_debug(ui_game_t* ui);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION (include in C++ source) ----------------------------------*/
#ifdef GAME_UI_IMPL
#ifndef __cplusplus
#error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef GAME_ASSERT
    #include <assert.h>
    #define GAME_ASSERT(c) assert(c)
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

static void _ui_game_draw_menu(ui_game_t* ui) {
    GAME_ASSERT(ui && ui->game);
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("System")) {
            ui_snapshot_menus(&ui->snapshot);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Info")) {
            ImGui::MenuItem("Video Hardware", 0, &ui->video.open);
            ImGui::MenuItem("Resource", 0, &ui->res.open);
            ImGui::MenuItem("Virtual machine", 0, &ui->vm.open);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("CPU Debugger", 0, &ui->dbg.ui.open);
            ImGui::MenuItem("Breakpoints", 0, &ui->dbg.ui.show_breakpoints);
            ImGui::MenuItem("Execution History", 0, &ui->dbg.ui.show_history);
            if (ImGui::BeginMenu("Disassembler")) {
                ImGui::MenuItem("Window #1", 0, &ui->dasm[0].open);
                ImGui::MenuItem("Window #2", 0, &ui->dasm[1].open);
                ImGui::MenuItem("Window #3", 0, &ui->dasm[2].open);
                ImGui::MenuItem("Window #4", 0, &ui->dasm[3].open);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ui_util_options_menu();
        ImGui::EndMainMenuBar();
    }
}

static void _ui_game_update_fbs(ui_game_t* ui) {
    for(int i=0; i<4; i++) {
        for(int j=0; j<GAME_WIDTH*GAME_HEIGHT; j++) {
            ui->video.pixel_buffer[j] = ui->game->gfx.palette[ui->game->gfx.fbs[i].buffer[j]];
        }
        ui->video.texture_cbs.update_cb(ui->video.tex_fb[i], ui->video.pixel_buffer, GAME_WIDTH*GAME_HEIGHT*sizeof(uint32_t));
    }
}

static void _ui_game_draw_vm(ui_game_t* ui) {
     if (!ui->vm.open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2((float)ui->vm.x, (float)ui->vm.y), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2((float)ui->vm.w, (float)ui->vm.h), ImGuiCond_Once);
    if (ImGui::Begin("Virtual machine", &ui->vm.open)) {
        if (ImGui::CollapsingHeader("Variables", ImGuiTreeNodeFlags_DefaultOpen)) {
            for(int i=0; i<256; i++) {
                const char* s = NULL;
                char tmp[16];
                switch(i) {
                    case GAME_VAR_RANDOM_SEED:
                    s = "RANDOM_SEED";
                    break;
                    case GAME_VAR_SCREEN_NUM:
                    s = "SCREEN_NUM";
                    break;
                    case GAME_VAR_LAST_KEYCHAR:
                    s = "LAST_KEYCHAR";
                    break;
                    case GAME_VAR_HERO_POS_UP_DOWN:
                    s = "HERO_POS_UP_DOWN";
                    break;
                    case GAME_VAR_MUSIC_SYNC:
                    s = "MUSIC_SYNC";
                    break;
                    case GAME_VAR_SCROLL_Y:
                    s = "SCROLL_Y";
                    break;
                    case GAME_VAR_HERO_ACTION:
                    s = "HERO_ACTION";
                    break;
                    case GAME_VAR_HERO_POS_JUMP_DOWN:
                    s = "GAME_VAR_HERO_POS_JUMP_DOWN";
                    break;
                    case GAME_VAR_HERO_POS_LEFT_RIGHT:
                    s = "GAME_VAR_HERO_POS_LEFT_RIGHT";
                    break;
                    case GAME_VAR_HERO_POS_MASK:
                    s = "GAME_VAR_HERO_POS_MASK";
                    break;
                    case GAME_VAR_HERO_ACTION_POS_MASK:
                    s = "GAME_VAR_HERO_ACTION_POS_MASK";
                    break;
                    case GAME_VAR_PAUSE_SLICES:
                    s = "GAME_VAR_PAUSE_SLICES";
                    break;
                    default:
                    snprintf(tmp, 16, "v%u", i);
                    s = tmp;
                    break;
                }
                ImGui::Text("%s = %d", s, ui->game->vm.vars[i]);
            }
        }
        if (ImGui::CollapsingHeader("Tasks", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##tasks", 6, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings)) {
                const ImU32 active_color = 0xFF00FFFF;
                const ImU32 normal_color = 0xFFFFFFFF;

                ImGui::TableSetupColumn("#");
                ImGui::TableSetupColumn("offset");
                ImGui::TableHeadersRow();

                for(int i=0; i<GAME_NUM_TASKS; i++) {
                    uint16_t offset = ui->game->vm.tasks[i].pc;

                    if(offset == 0xffff) continue;

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::PushID(i);
                    ImGui::PushStyleColor(ImGuiCol_Text, ui->game->vm.current_task == i?active_color:normal_color);
                    ImGui::Text("%u", i); ImGui::TableNextColumn();
                    ImGui::Text("%04X", offset); ImGui::TableNextColumn();
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::End();
}

static char* _convert_size(int size, char* str, size_t len) {
    static const char* suffix[] = {"B", "KB", "MB", "GB", "TB"};
    int s = size;
    float dblBytes = (float)size;
    int i = 0;
    if (s > 1024) {
        while ((s / 1024) > 0 && (i < 5)) {
            dblBytes = ((float)s) / 1024.f;
            s = s / 1024;
            i++;
        }
    }
    snprintf(str, len, "%.2f %s", dblBytes, suffix[i]);
    return str;
}

inline uint16_t _read_be_uint16(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 8) | b[1];
}

static void decode_amiga(const uint8_t *src, uint32_t *dst, uint32_t pal[16]) {
	static const int plane_size = GAME_WIDTH * GAME_HEIGHT / 8;
	for (int y = 0; y < GAME_HEIGHT; ++y) {
		for (int x = 0; x < GAME_WIDTH; x += 8) {
			for (int b = 0; b < 8; ++b) {
				const int mask = 1 << (7 - b);
				uint8_t color = 0;
				for (int p = 0; p < 4; ++p) {
					if (src[p * plane_size] & mask) {
						color |= 1 << p;
					}
				}
				*dst++ = pal[color];
			}
			++src;
		}
	}
}

static void _ui_game_get_pal(ui_game_t* ui, int res_id, int id, uint32_t pal[16]) {
    game_get_res_buf(ui->game, res_id, ui->res.buf);
    const uint8_t *p = ui->res.buf + id * 16 * sizeof(uint16_t);
    for (int i = 0; i < 16; ++i) {
        const uint16_t color = _read_be_uint16(p); p += 2;
        uint8_t r = (color >> 8) & 0xF;
        uint8_t g = (color >> 4) & 0xF;
        uint8_t b =  color       & 0xF;
        r = (r << 4) | r;
        g = (g << 4) | g;
        b = (b << 4) | b;
        ImColor c = ImColor(r, g, b);
        pal[i] = (ImU32)c;
    }
}

static void _ui_game_get_pal_for_res(ui_game_t* ui, int res_id, uint32_t pal[16]) {
    static uint8_t _res[] = {
        0x12, 0, 4,
        0x13, 0, 10,
        0x43, 4, 6,
        0x44, 4, 8,
        0x45, 4, 8,
        0x46, 4, 8,
        0x47, 0, 2,
        0x48, 8, 7,
        0x49, 8, 7,
        0x53, 0, 10,
        0x90, 6, 3,
        0x91, 6, 1
    };
    static uint8_t _res_ids[] = { 0x14, 0x17, 0x1a, 0x1d, 0x20, 0x23, 0x26, 0x29, 0x7d };

    for(int i=0; i<12; i++) {
        if(_res[i*3] == res_id) {
            if(!ui->res.data.pal_ovw) {
                ui->res.data.pal_idx = _res[i*3+2];
                ui->res.data.pal_res_idx = _res[i*3+1];
            }
            _ui_game_get_pal(ui, _res_ids[ui->res.data.pal_res_idx], ui->res.data.pal_idx, pal);
        }
    }
}

static void _ui_game_draw_sel_res(ui_game_t* ui) {
    game_mem_entry_t* e = &ui->game->res.mem_list[ui->res.selected];
    if(e->type == RT_PALETTE) {
        game_get_res_buf(ui->game, ui->res.selected, ui->res.buf);
        for (int num = 0; num < 32; ++num) {
            const uint8_t *p = ui->res.buf + num * 16 * sizeof(uint16_t);
            for (int i = 0; i < 16; ++i) {
                const uint16_t color = _read_be_uint16(p); p += 2;
                uint8_t r = (color >> 8) & 0xF;
                uint8_t g = (color >> 4) & 0xF;
                uint8_t b =  color       & 0xF;
                r = (r << 4) | r;
                g = (g << 4) | g;
                b = (b << 4) | b;
                ImColor c = ImColor(r, g, b);
                ImGui::PushID(i);
                ImGui::ColorEdit3("##pal_color", &c.Value.x, ImGuiColorEditFlags_NoInputs);
                ImGui::SameLine();
                ImGui::PopID();
            }
            ImGui::NewLine();
        }
    } else if(e->type == RT_BITMAP) {
        uint32_t pal[16];
        uint8_t buffer[GAME_WIDTH*GAME_HEIGHT/2];
        _ui_game_get_pal_for_res(ui, ui->res.selected, pal);
        if(game_get_res_buf(ui->game, ui->res.selected, buffer)) {
            static const char* _res_ids[] = { "0x14", "0x17", "0x1a", "0x1d", "0x20", "0x23", "0x26", "0x29", "0x7d" };
            decode_amiga(buffer, (uint32_t*)ui->res.buf, pal);
            ui->video.texture_cbs.update_cb(ui->res.tex_bmp, ui->res.buf, GAME_WIDTH*GAME_HEIGHT*sizeof(uint32_t));
            ImGui::Image(ui->res.tex_bmp, ImVec2(GAME_WIDTH, GAME_HEIGHT));
            ImGui::Checkbox("Palette overwrite", &ui->res.data.pal_ovw);
            ImGui::BeginDisabled(!ui->res.data.pal_ovw);
            ImGui::SliderInt("Palette res", &ui->res.data.pal_res_idx, 0, 8, _res_ids[ui->res.data.pal_res_idx]);
            ImGui::SliderInt("Palette id", &ui->res.data.pal_idx, 0, 15);
            ImGui::EndDisabled();
        } else {
            ImGui::Text("Not available");
        }
    }
}

static void _ui_game_draw_resources(ui_game_t* ui) {
     if (!ui->res.open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2((float)ui->res.x, (float)ui->res.y), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2((float)ui->res.w, (float)ui->res.h), ImGuiCond_Once);
    if (ImGui::Begin("Resources", &ui->res.open)) {

        static const char* labels[] = {"Sound", "Music", "Bitmap", "Palette", "Byte code", "Shape", "Bank" };
        for (int i = 0; i < 7; i++)
        {
            if (i > 0)
                ImGui::SameLine();
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(i / 7.0f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(i / 7.0f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(i / 7.0f, 0.8f, 0.8f));
            if(ImGui::Button(labels[i])) {
                ui->res.filters[i] = !ui->res.filters[i];
            }
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }

        if (ImGui::BeginTable("##resources", 6, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings)) {
            ImGui::TableSetupColumn("#");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Bank");
            ImGui::TableSetupColumn("Packed Size");
            ImGui::TableSetupColumn("Size");
            ImGui::TableHeadersRow();

            for(int i=0; i<ui->game->res.num_mem_list; i++) {
                game_mem_entry_t* e = &ui->game->res.mem_list[i];
                if(e->status == 0xFF) break;

                if(ui->res.filters[e->type]) continue;
                if(ui->game->res.data.banks[e->bank_num-1].size == 0) continue;

                ImGui::TableNextRow(0, 20.f);
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                char status_text[256];
                snprintf(status_text, 256, "%02X", i);
                if(ImGui::Selectable(status_text, ui->res.selected == i, ImGuiSelectableFlags_SpanAllColumns)) {
                    ui->res.selected = i;
                }
                ImGui::TableNextColumn();
                static const char* labels[]  = {"Sound", "Music", "Bitmap", "Palette", "Byte code", "Shape", "Bank"};
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::ColorButton("##color", ImColor::HSV(e->type / 7.0f, 0.6f, 0.6f), ImGuiColorEditFlags_NoTooltip); ImGui::SameLine(); ImGui::Text("%s", labels[e->type]);
                ImGui::PopStyleVar();

                ImGui::TableNextColumn();
                ImGui::Text("%02X", e->bank_num); ImGui::TableNextColumn();
                ImGui::Text("%s", _convert_size(e->packed_size, status_text, 256)); ImGui::TableNextColumn();
                ImGui::Text("%s", _convert_size(e->unpacked_size, status_text, 256));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::Separator();
        _ui_game_draw_sel_res(ui);
    }
    ImGui::End();
}

static void _ui_game_draw_video(ui_game_t* ui) {
    if (!ui->video.open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2((float)ui->video.x, (float)ui->video.y), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2((float)ui->video.w, (float)ui->video.h), ImGuiCond_Once);
    if (ImGui::Begin("Video", &ui->video.open)) {
        if (ImGui::CollapsingHeader("Palette", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (int col = 0; col < 16; col++) {
                ImGui::PushID(col);
                ImColor c = ImColor(ui->game->gfx.palette[col]);
                if(ImGui::ColorEdit3("##hw_color", &c.Value.x, ImGuiColorEditFlags_NoInputs)) {
                    ui->game->gfx.palette[col] = (ImU32)c;
                }
                ImGui::PopID();
                if (col != 7) {
                    ImGui::SameLine();
                }
            }
        }
        ImGui::NewLine();
        if (ImGui::CollapsingHeader("Frame buffers", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Current page: %d", ui->game->video.buffers[0]);
            _ui_game_update_fbs(ui);
            for(int i=0; i<4; i++) {
                const ImVec4 border_color = ui->game->video.buffers[0] == i ? ImGui::ColorConvertU32ToFloat4(0xFF30FF30) : ImVec4(0, 0, 0, 0);
                ImGui::Image(ui->video.tex_fb[i], ImVec2(GAME_WIDTH, GAME_HEIGHT), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), border_color);
                if (i != 1) {
                    ImGui::SameLine();
                }
            }
        }
    }
    ImGui::End();
}

static uint8_t _ui_raw_mem_read(int layer, uint16_t addr, bool* valid, void* user_data) {
    GAME_ASSERT(user_data);
    (void)layer;
    game_t* game = (game_t*) user_data;
    uint8_t* pc = game->res.seg_code;
    *valid = false;
    if (pc != NULL && addr >= 0 && addr < game->res.seg_code_size) {
        *valid = true;
        return pc[addr];
    }
    return 0;
}

void ui_game_init(ui_game_t* ui, const ui_game_desc_t* ui_desc) {
    GAME_ASSERT(ui && ui_desc);
    GAME_ASSERT(ui_desc->game);
    ui->game = ui_desc->game;
    ui_snapshot_init(&ui->snapshot, &ui_desc->snapshot);
    {
        ui_dbg_desc_t desc = {0};
        desc.title = "CPU Debugger";
        desc.x = 10;
        desc.y = 20;
        desc.read_cb = _ui_raw_mem_read;
        desc.texture_cbs = ui_desc->dbg_texture;
        desc.keys = ui_desc->dbg_keys;
        desc.user_data = ui->game;
        ui_dbg_init(&ui->dbg, &desc);
    }
    int x = 20, y = 20, dx = 10, dy = 10;
    {
        ui_dasm_desc_t desc = {0};
        desc.layers[0] = "System";
        desc.read_cb = _ui_raw_mem_read;
        desc.user_data = ui->game;
        static const char* titles[4] = { "Disassembler #1", "Disassembler #2", "Disassembler #2", "Dissassembler #3" };
        for (int i = 0; i < 4; i++) {
            desc.title = titles[i]; desc.x = x; desc.y = y;
            ui_dasm_init(&ui->dasm[i], &desc);
            x += dx; y += dy;
        }
    }
    {
        ui->video.texture_cbs = ui_desc->dbg_texture;
        ui->video.x = 10;
        ui->video.y = 20;
        ui->video.w = 562;
        ui->video.h = 568;
        for(int i=0; i<4; i++) {
            ui->video.tex_fb[i] = ui->video.texture_cbs.create_cb(GAME_WIDTH, GAME_HEIGHT);
        }
    }
    {
        ui->res.x = 10;
        ui->res.y = 20;
        ui->res.w = 562;
        ui->res.h = 568;
    }
    {
        ui->vm.x = 10;
        ui->vm.y = 20;
        ui->vm.w = 562;
        ui->vm.h = 568;
    }
    ui->res.tex_bmp = ui->video.texture_cbs.create_cb(GAME_WIDTH, GAME_HEIGHT);
}

void ui_game_discard(ui_game_t* ui) {
    GAME_ASSERT(ui && ui->game);
    ui->video.texture_cbs.destroy_cb(ui->res.tex_bmp);
    for(int i=0; i<4; i++) {
        ui->video.texture_cbs.destroy_cb(ui->video.tex_fb[i]);
    }
    for (int i = 0; i < 4; i++) {
        ui_dasm_discard(&ui->dasm[i]);
    }
    ui_dbg_discard(&ui->dbg);
    ui->game = 0;
}

void ui_game_draw(ui_game_t* ui) {
    GAME_ASSERT(ui && ui->game);
    _ui_game_draw_menu(ui);
    _ui_game_draw_resources(ui);
    _ui_game_draw_video(ui);
    _ui_game_draw_vm(ui);
    for (int i = 0; i < 4; i++) {
        ui_dasm_draw(&ui->dasm[i]);
    }
    ui_dbg_draw(&ui->dbg);
}

game_debug_t ui_game_get_debug(ui_game_t* ui) {
    GAME_ASSERT(ui);
    game_debug_t res = {};
    res.callback.func = (game_debug_func_t)ui_dbg_tick;
    res.callback.user_data = &ui->dbg;
    res.stopped = &ui->dbg.dbg.stopped;
    return res;
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif /* GAME_UI_IMPL */
