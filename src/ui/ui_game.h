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
    uint32_t pixel_buffer[320*200];
} ui_game_video_t;

typedef struct {
    int x, y;
    int w, h;
    bool open;
    game_mem_entry_t* res;
} ui_game_res_t;

typedef struct {
    int x, y;
    int w, h;
    bool open;
    int16_t* vars;
} ui_game_script_t;

typedef struct {
    game_t* game;
    ui_dbg_texture_callbacks_t dbg_texture;     // debug texture create/update/destroy callbacks
} ui_game_desc_t;

typedef struct {
    game_t* game;
    ui_game_video_t video;
    ui_game_res_t res;
    ui_game_script_t script;
    ui_dasm_t dasm[4];
    ui_dbg_t dbg;
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
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Info")) {
            ImGui::MenuItem("Video Hardware", 0, &ui->video.open);
            ImGui::MenuItem("Resource", 0, &ui->res.open);
            ImGui::MenuItem("Script", 0, &ui->script.open);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("CPU Debugger", 0, &ui->dbg.ui.open);
            ImGui::MenuItem("Breakpoints", 0, &ui->dbg.ui.show_breakpoints);
            ImGui::MenuItem("Execution History", 0, &ui->dbg.ui.show_history);
            ImGui::MenuItem("Memory Heatmap", 0, &ui->dbg.ui.show_heatmap);
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
        for(int j=0; j<320*200; j++) {
            ui->video.pixel_buffer[j] = ui->game->palette[ui->game->fbs[i].buffer[j]];
        }
        ui->video.texture_cbs.update_cb(ui->video.tex_fb[i], ui->video.pixel_buffer, 320*200*sizeof(uint32_t));
    }
}

static void _ui_game_draw_script(ui_game_t* ui) {
     if (!ui->script.open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2((float)ui->script.x, (float)ui->script.y), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2((float)ui->script.w, (float)ui->script.h), ImGuiCond_Once);
    if (ImGui::Begin("Script", &ui->script.open)) {
        for(int i=0; i<256; i++) {
            const char* s = NULL;
            char tmp[16];
            switch(i) {
                case Script::VAR_RANDOM_SEED:
                s = "RANDOM_SEED";
                break;
                case Script::VAR_SCREEN_NUM:
                s = "SCREEN_NUM";
                break;
                case Script::VAR_LAST_KEYCHAR:
                s = "LAST_KEYCHAR";
                break;
                case Script::VAR_HERO_POS_UP_DOWN:
                s = "HERO_POS_UP_DOWN";
                break;
                case Script::VAR_MUSIC_SYNC:
                s = "MUSIC_SYNC";
                break;
                case Script::VAR_SCROLL_Y:
                s = "SCROLL_Y";
                break;
                case Script::VAR_HERO_ACTION:
                s = "HERO_ACTION";
                break;
                case Script::VAR_HERO_POS_JUMP_DOWN:
                s = "VAR_HERO_POS_JUMP_DOWN";
                break;
                case Script::VAR_HERO_POS_LEFT_RIGHT:
                s = "VAR_HERO_POS_LEFT_RIGHT";
                break;
                case Script::VAR_HERO_POS_MASK:
                s = "VAR_HERO_POS_MASK";
                break;
                case Script::VAR_HERO_ACTION_POS_MASK:
                s = "VAR_HERO_ACTION_POS_MASK";
                break;
                case Script::VAR_PAUSE_SLICES:
                s = "VAR_PAUSE_SLICES";
                break;
                default:
                sprintf(tmp, "VAR%u", i);
                s = tmp;
                break;
            }
            ImGui::Text("%s = %d", s, ui->script.vars[i]);
        }
    }
    ImGui::End();
}

static void _ui_game_draw_resources(ui_game_t* ui) {
     if (!ui->res.open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2((float)ui->res.x, (float)ui->res.y), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2((float)ui->res.w, (float)ui->res.h), ImGuiCond_Once);
    if (ImGui::Begin("Resources", &ui->res.open)) {

        if (ImGui::BeginTable("##resources", 6, ImGuiTableFlags_Resizable|ImGuiTableFlags_Sortable)) {

            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Status");
            ImGui::TableSetupColumn("Rank");
            ImGui::TableSetupColumn("Bank");
            ImGui::TableSetupColumn("Packed Size");
            ImGui::TableSetupColumn("Size");
            ImGui::TableHeadersRow();

            for(int i=0; i<GAME_ENTRIES_COUNT_20TH; i++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                game_mem_entry_t* e = &ui->res.res[i];
                if(e->status == 0xFF) break;
                Resource::ResType t = (Resource::ResType)e->type;
                switch(t) {
                    case Resource::RT_SOUND: ImGui::Text("Sound"); break;
                    case Resource::RT_MUSIC: ImGui::Text("Music"); break;
                    case Resource::RT_BITMAP: ImGui::Text("Bitmap"); break;
                    case Resource::RT_PALETTE: ImGui::Text("Palette"); break;
                    case Resource::RT_BYTECODE: ImGui::Text("Byte code"); break;
                    case Resource::RT_SHAPE: ImGui::Text("Shape"); break;
                    case Resource::RT_BANK: ImGui::Text("Bank"); break;
                }
                ImGui::TableNextColumn();
                ImGui::Text("%u", e->status);
                ImGui::TableNextColumn();
                ImGui::Text("%u", e->rankNum);
                ImGui::TableNextColumn();
                ImGui::Text("%u", e->bankNum);
                ImGui::TableNextColumn();
                ImGui::Text("%u", e->packedSize);
                ImGui::TableNextColumn();
                ImGui::Text("%u", e->unpackedSize);
            }
            ImGui::EndTable();
        }
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
                ImColor c = ImColor(ui->game->palette[col]);
                if(ImGui::ColorEdit3("##hw_color", &c.Value.x, ImGuiColorEditFlags_NoInputs)) {
                    ui->game->palette[col] = (ImU32)c;
                }
                ImGui::PopID();
                if (col != 4) {
                    ImGui::SameLine();
                }
            }
        }
        ImGui::NewLine();
        if (ImGui::CollapsingHeader("Frame buffers", ImGuiTreeNodeFlags_DefaultOpen)) {
            _ui_game_update_fbs(ui);
            for(int i=0; i<4; i++) {
                ImGui::Image(ui->video.tex_fb[i], ImVec2(320, 200));
                if (i != 1) {
                    ImGui::SameLine();
                }
            }
        }
    }
    ImGui::End();
}

static uint8_t _ui_raw_mem_read(int layer, uint16_t addr, void* user_data) {
    GAME_ASSERT(user_data);
    (void)layer;
    ui_game_t* ui = (ui_game_t*) user_data;
    uint8_t* pc = game_get_pc(ui->game);
    return pc[addr];
}

void ui_game_init(ui_game_t* ui, const ui_game_desc_t* ui_desc) {
    GAME_ASSERT(ui && ui_desc);
    GAME_ASSERT(ui_desc->game);
    ui->game = ui_desc->game;
     {
        ui_dbg_desc_t desc = {0};
        desc.title = "CPU Debugger";
        desc.x = 10;
        desc.y = 20;
        desc.game = ui->game;
        desc.read_cb = _ui_raw_mem_read;
        desc.texture_cbs = ui_desc->dbg_texture;
        //desc.keys = ui_desc->dbg_keys;
        desc.user_data = ui->game;
        ui_dbg_init(&ui->dbg, &desc);
    }
    {
        ui->video.texture_cbs = ui_desc->dbg_texture;
        ui->video.x = 10;
        ui->video.y = 20;
        ui->video.w = 562;
        ui->video.h = 568;
        for(int i=0; i<4; i++) {
            ui->video.tex_fb[i] = ui->video.texture_cbs.create_cb(320, 200);
        }
    }
    {
        ui->res.x = 10;
        ui->res.y = 20;
        ui->res.w = 562;
        ui->res.h = 568;
        game_get_resources(ui->game, &ui->res.res);
    }
    {
        ui->script.x = 10;
        ui->script.y = 20;
        ui->script.w = 562;
        ui->script.h = 568;
        game_get_vars(ui->game, &ui->script.vars);
    }
    {
        ui_dasm_desc_t desc = {0};
        desc.layers[0] = "System";
        desc.start_addr = 0;
        desc.read_cb = _ui_raw_mem_read;
        desc.user_data = ui;
        static const char* titles[4] = { "Disassembler #1", "Disassembler #2", "Disassembler #2", "Dissassembler #3" };
        int x = 20, y = 20, dx = 10, dy = 10;
        for (int i = 0; i < 4; i++) {
            desc.title = titles[i]; desc.x = x; desc.y = y;
            ui_dasm_init(&ui->dasm[i], &desc);
            x += dx; y += dy;
        }
    }
}

void ui_game_discard(ui_game_t* ui) {
    GAME_ASSERT(ui && ui->game);
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
    _ui_game_draw_script(ui);
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
