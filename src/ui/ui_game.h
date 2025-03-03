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

#define UI_RES_MAX_POLY_OFFSETS (2769)

typedef struct {
    int x, y;
    int w, h;
    bool open;
    ui_dbg_texture_callbacks_t texture_cbs;
    ui_texture_t tex_fb[4];
    uint32_t pixel_buffer[GAME_WIDTH*GAME_HEIGHT];
} ui_game_video_t;


typedef struct {
    uint16_t offsets[UI_RES_MAX_POLY_OFFSETS];
    uint16_t num_offset;
} ui_res_poly_info_t;

typedef struct {
    int x, y;
    int w, h;
    bool open;
    ui_texture_t tex_bmp;
    ui_texture_t tex_fb;
    int selected;
    bool filters[7]; // filter each resource types
    struct {
        struct {
            uint8_t buf[GAME_WIDTH*GAME_HEIGHT*4];
        } bmp;
        struct {
            uint8_t buf[2048];
            int idx;
            int res_idx;
            bool ovw;
        } pal;
        struct {
            ui_res_poly_info_t info;
            uint8_t buf[65536];
            uint8_t img[GAME_WIDTH*GAME_HEIGHT];
            uint16_t offset;
            int pos[2], zoom;
            int pal_idx;
        } poly;
        struct {
            ui_dasm_t dasm;
            uint8_t buf[65536];
            uint16_t size;
        } code;
    } data;
} ui_game_res_t;

typedef struct {
    int x, y;
    int w, h;
    bool open;
} ui_game_vars_t;

typedef struct {
    int x, y;
    int w, h;
    bool open;
} ui_game_tasks_t;

typedef struct {
    int         x, y;
    int         w, h;
    bool        open;
} ui_game_inputs_t;

typedef struct {
    int             x, y;
    int             w, h;
    bool            open;
    const float*    sample_buffer;  /* pointer to audio sample buffer */
    int*            num_samples;    /* max number of samples in sample buffer */
} ui_game_audio_t;

typedef struct {
    game_t* game;
    ui_dbg_texture_callbacks_t dbg_texture;     // debug texture create/update/destroy callbacks
    ui_dbg_keys_desc_t dbg_keys;                // user-defined hotkeys for ui_dbg_t
    ui_snapshot_desc_t snapshot;                // snapshot ui setup params
} ui_game_desc_t;

typedef struct {
    game_t*             game;
    ui_game_video_t     video;
    ui_display_t        display;
    ui_game_res_t       res;
    ui_game_vars_t      vars;
    ui_game_tasks_t     tasks;
    ui_game_inputs_t    inputs;
    ui_game_audio_t     audio;
    ui_dbg_t            dbg;
    ui_snapshot_t       snapshot;
    ui_dasm_t           dasm[4];
} ui_game_t;

typedef struct {
    ui_display_frame_t display;
} ui_game_frame_t;

void ui_game_init(ui_game_t* ui, const ui_game_desc_t* desc);
void ui_game_discard(ui_game_t* ui);
void ui_game_draw(ui_game_t* ui, const ui_game_frame_t* frame);
game_debug_t ui_game_get_debug(ui_game_t* ui);
void ui_game_save_settings(ui_game_t* ui, ui_settings_t* settings);
void ui_game_load_settings(ui_game_t* ui, const ui_settings_t* settings);

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

#define _MIN(v1, v2) ((v1 < v2) ? v1 : v2)

#define _GAME_QUAD_STRIP_MAX_VERTICES    (70)

typedef struct {
	int16_t x, y;
} _game_point_t;

typedef struct {
	uint8_t num_vertices;
	_game_point_t vertices[_GAME_QUAD_STRIP_MAX_VERTICES];
} _game_quad_strip_t;

static void _ui_game_draw_menu(ui_game_t* ui) {
    GAME_ASSERT(ui && ui->game);
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("System")) {
            ui_snapshot_menus(&ui->snapshot);
            if (ImGui::BeginMenu("Select Part")) {
                const int parts[] = {GAME_PART_INTRO, GAME_PART_WATER, GAME_PART_PRISON, GAME_PART_CITE, GAME_PART_ARENE, GAME_PART_LUXE, GAME_PART_FINAL, GAME_PART_PASSWORD, GAME_PART_COPY_PROTECTION};
                const char* part_names[] = {"Intro", "Water", "Prison", "Cite", "Arene", "Luxe", "Final", "Password", "Copy Protection"};
                const int selected_part = game_get_selected_part(ui->game);
                for(int i=0; i<9; i++) {
                    bool selected = parts[i] == selected_part;
                    if(!game_part_exists(ui->game, parts[i]))
                        continue;
                    if(ImGui::MenuItem(part_names[i], 0, &selected)) {
                        game_select_part(ui->game, parts[i]);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Info")) {
            ImGui::MenuItem("Video Hardware", 0, &ui->video.open);
            ImGui::MenuItem("Resource", 0, &ui->res.open);
            ImGui::MenuItem("Inputs", 0, &ui->inputs.open);
            ImGui::MenuItem("Audio", 0, &ui->audio.open);
            ImGui::MenuItem("Display", 0, &ui->display.open);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("CPU Debugger", 0, &ui->dbg.ui.open);
            ImGui::MenuItem("Breakpoints", 0, &ui->dbg.ui.show_breakpoints);
            ImGui::MenuItem("Tasks", 0, &ui->tasks.open);
            ImGui::MenuItem("Variables", 0, &ui->vars.open);
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
    if (!ui->vars.open) {
        return;
    }
    struct { int var; const char* str; } var_names[] = {
        {GAME_VAR_RANDOM_SEED, "RANDOM_SEED"},
        {GAME_VAR_SCREEN_NUM, "SCREEN_NUM"},
        {GAME_VAR_LAST_KEYCHAR, "LAST_KEYCHAR"},
        {GAME_VAR_HERO_POS_UP_DOWN, "HERO_POS_UP_DOWN"},
        {GAME_VAR_MUSIC_SYNC, "MUSIC_SYNC"},
        {GAME_VAR_SCROLL_Y, "SCROLL_Y"},
        {GAME_VAR_HERO_ACTION, "HERO_ACTION"},
        {GAME_VAR_HERO_POS_JUMP_DOWN, "HERO_POS_JUMP_DOWN"},
        {GAME_VAR_HERO_POS_LEFT_RIGHT, "HERO_POS_LEFT_RIGHT"},
        {GAME_VAR_HERO_POS_MASK, "HERO_POS_MASK"},
        {GAME_VAR_HERO_ACTION_POS_MASK, "HERO_ACTION_POS_MASK"},
        {GAME_VAR_PAUSE_SLICES, "PAUSE_SLICES"},
    };
    const int var_names_size = sizeof(var_names) / sizeof(var_names[0]);

    ImGui::SetNextWindowPos(ImVec2((float)ui->vars.x, (float)ui->vars.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2((float)ui->vars.w, (float)ui->vars.h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Variables", &ui->vars.open)) {
        if (ImGui::BeginTable("##vars", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings)) {
            char tmp[16];
            for(int i=0; i<256; i++) {
                const char* s = NULL;
                for(int j=0; j<var_names_size; j++) {
                    if(var_names[j].var == i) {
                        s = var_names[j].str;
                        break;
                    }
                }
                if(!s) {
                    snprintf(tmp, 16, "v%u", i);
                    s = tmp;
                }
                
                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", s);
                ImGui::TableNextColumn();
                ImGui::DragScalar("##value", ImGuiDataType_S16, &ui->game->vm.vars[i]);
                ImGui::TableNextColumn();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

static void _ui_game_draw_tasks(ui_game_t* ui) {
    if (!ui->tasks.open) {
        return;
    }

    const ImU32 active_color = 0xFF00FFFF;
    const ImU32 normal_color = 0xFFFFFFFF;

    ImGui::SetNextWindowPos(ImVec2((float)ui->tasks.x, (float)ui->tasks.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2((float)ui->tasks.w, (float)ui->tasks.h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Tasks", &ui->tasks.open)) {
        for(int i=0; i<GAME_NUM_TASKS; i++) {
            uint16_t offset = ui->game->vm.tasks[i].pc;
            if(offset == 0xffff) continue;
            
            const bool is_active = ui->game->vm.current_task == i;

            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Text, is_active ? active_color : normal_color);
            ImGui::Text("Task: %02d  Offset: %04X", i, offset);
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::Separator();
        if(ui->game->vm.stack_ptr) {
            ImGui::Text("Callstack (Task %02d)", ui->game->vm.current_task);
            uint16_t offset = ui->game->vm.tasks[ui->game->vm.current_task].pc;
            ImGui::Text("Offset: %04X", offset);
            for(int j=0; j<ui->game->vm.stack_ptr; j++) {
                ImGui::Text("Offset: %04X", ui->game->vm.stack_calls[j]);
            }
        }
    }
    ImGui::End();
}

static void _ui_game_draw_inputs(ui_game_t* ui) {
    if (!ui->inputs.open) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2((float)ui->inputs.x, (float)ui->inputs.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2((float)ui->inputs.w, (float)ui->inputs.h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Inputs", &ui->inputs.open)) {
        ImGui::Text("dir_mask: %u", ui->game->input.dir_mask);
        ImGui::Text("action: %s", ui->game->input.action ? "ON" : "OFF");
        ImGui::Text("code: %s", ui->game->input.code ? "ON" : "OFF");
        ImGui::Text("pause: %s", ui->game->input.pause ? "ON" : "OFF");
        ImGui::Text("quit: %s", ui->game->input.quit ? "ON" : "OFF");
        ImGui::Text("back: %s", ui->game->input.back ? "ON" : "OFF");
        ImGui::Text("last_char: %c", ui->game->input.last_char);
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
    game_get_res_buf(ui->game, res_id, ui->res.data.pal.buf);
    const uint8_t *p = ui->res.data.pal.buf + id * 16 * sizeof(uint16_t);
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
    static const uint8_t _res[] = {
        0x12, 0, 4,
        0x13, 0, 10,
        0x43, 4, 6,
        0x44, 4, 8,
        0x45, 4, 8,
        0x46, 4, 8,
        0x47, 0, 2,
        0x48, 3, 25,
        0x49, 3, 25,
        0x53, 0, 10,
        0x90, 6, 3,
        0x91, 6, 1
    };
    static const uint8_t _res_ids[] = { 0x14, 0x17, 0x1a, 0x1d, 0x20, 0x23, 0x26, 0x29, 0x7d };

    for(int i=0; i<12; i++) {
        if(_res[i*3] == res_id) {
            if(!ui->res.data.pal.ovw) {
                ui->res.data.pal.idx = _res[i*3+2];
                ui->res.data.pal.res_idx = _res[i*3+1];
            }
            _ui_game_get_pal(ui, _res_ids[ui->res.data.pal.res_idx], ui->res.data.pal.idx, pal);
        }
    }
}

// poly offsets
static uint8_t* _ui_game_get_polygon_offsets(uint8_t* p) {
	p += 2;
	uint16_t num_vertices = *p++;
	if ((num_vertices & 1) != 0) {
		return p;
	}
    p += (num_vertices * 2);
	return p;
}

static uint8_t* _ui_game_get_shape_offsets(uint8_t* p) {
    p += 2;
    int16_t n = *p++;
    for ( ; n >= 0; --n) {
        uint16_t offset = (p[0] << 8) | p[1];
        p += 4;
        if (offset & 0x8000) {
            p += 2;
        }
        offset <<= 1;
    }
    return p;
}

static void _ui_game_get_poly_offsets(uint8_t* p, uint16_t size, ui_res_poly_info_t* info) {
    const uint8_t* p_start = p;
    const uint8_t* p_end = p + size;
    info->num_offset = 0;
    do {
        info->offsets[info->num_offset++] = p - p_start;
        uint8_t i = *p++;
        if (i >= 0xc0) {
            p = _ui_game_get_polygon_offsets(p);
        } else {
            i &= 0x3f;
            if (i == 2) {
            	p = _ui_game_get_shape_offsets(p);
            }
        }
        if(info->num_offset == (UI_RES_MAX_POLY_OFFSETS+1)) { assert(false); }
    } while(p < p_end);
}

// poly drawing
#define _GFX_COL_ALPHA      (0x10) // transparent pixel (OR'ed with 0x8)
#define _GFX_COL_PAGE       (0x11) // buffer 0 pixel
#define _MIN(v1, v2) ((v1 < v2) ? v1 : v2)
#define _MAX(v1, v2) ((v1 > v2) ? v1 : v2)

typedef void (*_gfx_draw_line_t)(ui_game_t* ui, int16_t x1, int16_t x2, int16_t y, uint8_t col);

static uint32_t _calc_step(const _game_point_t* p1, const _game_point_t* p2, uint16_t* dy) {
    *dy = p2->y - p1->y;
    uint16_t delta = (*dy <= 1) ? 1 : *dy;
    return ((p2->x - p1->x) * (0x4000 / delta)) << 2;
}


static void _game_gfx_draw_line_p(ui_game_t* ui, int16_t x1, int16_t x2, int16_t y, uint8_t color) {
    // TODO ?
    (void)ui;(void)x1;(void)x2;(void)y;(void)color;
}

static void _game_gfx_draw_line_n(ui_game_t* ui, int16_t x1, int16_t x2, int16_t y, uint8_t color) {
    const int16_t xmax = _MAX(x1, x2);
    const int16_t xmin = _MIN(x1, x2);
    const int w = xmax - xmin + 1;
    const int offset = (y * GAME_WIDTH + xmin);
    memset(ui->res.data.poly.img + offset, color, w);
}

static void _game_gfx_draw_line_trans(ui_game_t* ui, int16_t x1, int16_t x2, int16_t y, uint8_t color) {
    (void)color;
    const int16_t xmax = _MAX(x1, x2);
    const int16_t xmin = _MIN(x1, x2);
    const int w = xmax - xmin + 1;
    const int offset = (y * GAME_WIDTH + xmin);
    for (int i = 0; i < w; ++i) {
        ui->res.data.poly.img[offset + i] |= 8;
    }
}

static void _game_gfx_draw_polygon(ui_game_t* ui, uint8_t color, const _game_quad_strip_t* quadStrip) {
    const _game_quad_strip_t* qs = quadStrip;

    int i = 0;
    int j = qs->num_vertices - 1;

    int16_t x2 = qs->vertices[i].x;
    int16_t x1 = qs->vertices[j].x;
    int16_t hliney = _MIN(qs->vertices[i].y, qs->vertices[j].y);

    ++i;
    --j;

    _gfx_draw_line_t pdl;
    switch (color) {
    default:
        pdl = &_game_gfx_draw_line_n;
        break;
    case _GFX_COL_PAGE:
        pdl = &_game_gfx_draw_line_p;
        break;
    case _GFX_COL_ALPHA:
        pdl = &_game_gfx_draw_line_trans;
        break;
    }

    uint32_t cpt1 = x1 << 16;
    uint32_t cpt2 = x2 << 16;

    int numVertices = qs->num_vertices;
    while (1) {
        numVertices -= 2;
        if (numVertices == 0) {
            return;
        }
        uint16_t h;
        uint32_t step1 = _calc_step(&qs->vertices[j + 1], &qs->vertices[j], &h);
        uint32_t step2 = _calc_step(&qs->vertices[i - 1], &qs->vertices[i], &h);

        ++i;
        --j;

        cpt1 = (cpt1 & 0xFFFF0000) | 0x7FFF;
        cpt2 = (cpt2 & 0xFFFF0000) | 0x8000;

        if (h == 0) {
            cpt1 += step1;
            cpt2 += step2;
        } else {
            while (h--) {
                if (hliney >= 0) {
                    x1 = cpt1 >> 16;
                    x2 = cpt2 >> 16;
                    if (x1 < GAME_WIDTH && x2 >= 0) {
                        if (x1 < 0) x1 = 0;
                        if (x2 >= GAME_WIDTH) x2 = GAME_WIDTH - 1;
                        (*pdl)(ui, x1, x2, hliney, color);
                    }
                }
                cpt1 += step1;
                cpt2 += step2;
                ++hliney;
                if (hliney >= GAME_HEIGHT) return;
            }
        }
    }
}

static void _game_gfx_draw_point(ui_game_t* ui, int16_t x, int16_t y, uint8_t color) {
    const int offset = (y * GAME_WIDTH + x);
    uint32_t* buf = (uint32_t*)ui->res.data.poly.img;
    switch (color) {
    case _GFX_COL_ALPHA:
        buf[offset] |= 8;
        break;
    case _GFX_COL_PAGE:
        buf[offset] = color;
        break;
    default:
        buf[offset] = color;
        break;
    }
}

static uint8_t* _game_video_fill_polygon(ui_game_t* ui, uint8_t* p, uint16_t color, uint16_t zoom, const _game_point_t *pt) {
    uint16_t bbw = (*p++) * zoom / 64;
    uint16_t bbh = (*p++) * zoom / 64;

    int16_t x1 = pt->x - bbw / 2;
    int16_t x2 = pt->x + bbw / 2;
    int16_t y1 = pt->y - bbh / 2;
    int16_t y2 = pt->y + bbh / 2;

    if (x1 >= GAME_WIDTH || x2 < 0 || y1 >= GAME_HEIGHT || y2 < 0)
        return p;

    _game_quad_strip_t qs;
    qs.num_vertices = *p++;
    if ((qs.num_vertices & 1) != 0) {
        return p;
    }
    GAME_ASSERT(qs.num_vertices < GAME_QUAD_STRIP_MAX_VERTICES);

    for (int i = 0; i < qs.num_vertices; ++i) {
        _game_point_t *v = &qs.vertices[i];
        v->x = x1 + (*p++) * zoom / 64;
        v->y = y1 + (*p++) * zoom / 64;
    }

    if (qs.num_vertices == 4 && bbw == 0 && bbh <= 1) {
        _game_gfx_draw_point(ui, pt->x, pt->y, color);
    } else {
        _game_gfx_draw_polygon(ui, color, &qs);
    }
    return p;
}

static void _game_video_draw_shape(ui_game_t* ui, uint8_t* p, uint8_t color, uint16_t zoom, const _game_point_t *pt);

static uint8_t* _game_video_draw_shape_parts(ui_game_t* ui, uint8_t* p, uint16_t zoom, const _game_point_t *pgc) {
    _game_point_t pt;
    pt.x = pgc->x - *p++ * zoom / 64;
    pt.y = pgc->y - *p++ * zoom / 64;
    int16_t n = *p++;
    for ( ; n >= 0; --n) {
        uint16_t offset = (p[0] << 8) | p[1]; p += 2;
        _game_point_t po;
        po.x = pt.x; po.y = pt.y;
        po.x += *p++ * zoom / 64;
        po.y += *p++ * zoom / 64;
        uint16_t color = 0xFF;
        if (offset & 0x8000) {
            color = *p++;
            p++;
            color &= 0x7F;
        }
        offset <<= 1;
        uint8_t *bak = p;
        _game_video_draw_shape(ui, ui->res.data.poly.buf + offset, color, zoom, &po);
        p = bak;
    }
    return p;
}

static void _game_video_draw_shape(ui_game_t* ui, uint8_t* p, uint8_t color, uint16_t zoom, const _game_point_t *pt) {
    uint8_t i = *p++;
    if (i >= 0xC0) {
        if (color & 0x80) {
            color = i & 0x3F;
        }
        p = _game_video_fill_polygon(ui, p, color, zoom, pt);
    } else {
        i &= 0x3F;
        if (i == 2) {
            p = _game_video_draw_shape_parts(ui, p, zoom, pt);
        }
    }
}

static void _ui_game_get_palette(uint8_t* buf, int pal_idx, ImColor* pal) {
    const uint8_t *p = buf + pal_idx * 16 * sizeof(uint16_t);
    for (int i = 0; i < 16; ++i) {
        const uint16_t color = _read_be_uint16(p); p += 2;
        uint8_t r = (color >> 8) & 0xF;
        uint8_t g = (color >> 4) & 0xF;
        uint8_t b =  color       & 0xF;
        r = (r << 4) | r;
        g = (g << 4) | g;
        b = (b << 4) | b;
        pal[i] = ImColor(r, g, b);
    }
}

static void _ui_game_draw_sel_res(ui_game_t* ui) {
    game_mem_entry_t* e = &ui->game->res.mem_list[ui->res.selected];
    ui->res.data.code.dasm.open = (e->type == RT_BYTECODE);
    if(e->type == RT_PALETTE) {
        game_get_res_buf(ui->game, ui->res.selected, ui->res.data.pal.buf);
        // display 32 palettes
        for (int num = 0; num < 32; ++num) {
            // 1 palette is 16 colors
            ImColor pal[16];
            _ui_game_get_palette(ui->res.data.pal.buf, num, pal);
            ImGui::Text("%02d", num);
            ImGui::SameLine();
            for (int i = 0; i < 16; ++i) {
                ImGui::PushID(i);
                ImGui::ColorEdit3("##pal_color", &pal[i].Value.x, ImGuiColorEditFlags_NoInputs);
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
            decode_amiga(buffer, (uint32_t*)ui->res.data.bmp.buf, pal);
            ui->video.texture_cbs.update_cb(ui->res.tex_bmp, ui->res.data.bmp.buf, GAME_WIDTH*GAME_HEIGHT*sizeof(uint32_t));
            ImGui::Image(ui->res.tex_bmp, ImVec2(GAME_WIDTH, GAME_HEIGHT));
            ImGui::Checkbox("Palette overwrite", &ui->res.data.pal.ovw);
            ImGui::BeginDisabled(!ui->res.data.pal.ovw);
            ImGui::SliderInt("Palette res", &ui->res.data.pal.res_idx, 0, 8, _res_ids[ui->res.data.pal.res_idx]);
            ImGui::SliderInt("Palette id", &ui->res.data.pal.idx, 0, 32);
            ImGui::EndDisabled();
        } else {
            ImGui::Text("Not available");
        }
    } else if(e->type == RT_SHAPE || e->type == RT_BANK) {
        // get polygon offsets
        if(game_get_res_buf(ui->game, ui->res.selected, ui->res.data.poly.buf)) {
            const float line_height = ImGui::GetTextLineHeight();
            ImGui::Text("Polygon offsets");
            ImGui::Separator();
            ImGui::BeginChild("##poly_offsets", ImVec2(ImGui::GetContentRegionAvail()[0], 10 * line_height), false);
            _ui_game_get_poly_offsets(ui->res.data.poly.buf, e->unpacked_size, &ui->res.data.poly.info);
            char offset_txt[5] = { 0 };
            for(int i=0; i<ui->res.data.poly.info.num_offset; i++) {
                snprintf(offset_txt, sizeof(offset_txt), "%04X", ui->res.data.poly.info.offsets[i]);
                ImGui::PushID(i);
                bool selected = ui->res.data.poly.offset == ui->res.data.poly.info.offsets[i];
                if(ImGui::Selectable(offset_txt, selected)) {
                    ui->res.data.poly.offset = ui->res.data.poly.info.offsets[i];
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        } else {
            ImGui::Text("Not available");
        }
        // draw selected polygon(s)
        if(ui->res.data.poly.offset != 0xffff) {
            _game_point_t pt;
            pt.x = (int16_t)ui->res.data.poly.pos[0]; pt.y = (int16_t)ui->res.data.poly.pos[1];
            memset(ui->res.data.poly.img, 0, GAME_WIDTH*GAME_HEIGHT);
            _game_video_draw_shape(ui, ui->res.data.poly.buf + ui->res.data.poly.offset, 0xff, ui->res.data.poly.zoom, &pt);
            ImColor pal[16];
            const int res_pal_index = ui->res.selected - 2;
            game_get_res_buf(ui->game, res_pal_index, ui->res.data.pal.buf);
            _ui_game_get_palette(ui->res.data.pal.buf, ui->res.data.poly.pal_idx, pal);
            uint32_t* buf = (uint32_t*)ui->res.data.bmp.buf;
            for(int i=0;i<GAME_WIDTH*GAME_HEIGHT;i++) {
                buf[i] = pal[ui->res.data.poly.img[i]];
            }
            ui->video.texture_cbs.update_cb(ui->res.tex_fb, buf, GAME_WIDTH*GAME_HEIGHT*sizeof(uint32_t));
            ImGui::Separator();
            ImGui::Image(ui->res.tex_fb, ImVec2(GAME_WIDTH, GAME_HEIGHT));
            ImGui::Separator();
            ImGui::SliderInt("palette", &ui->res.data.poly.pal_idx, 0, 32, "%02X");
            ImGui::SliderInt2("pos", &ui->res.data.poly.pos[0], -480, 480);
            ImGui::SliderInt("zoom", &ui->res.data.poly.zoom, 1, 320);
        }
    } else if(e->type == RT_BYTECODE) {
        game_get_res_buf(ui->game, ui->res.selected, ui->res.data.code.buf);
        ui->res.data.code.size = e->unpacked_size;
    } else {
        ImGui::Text("No preview");
    }
}

static void _ui_game_draw_resources(ui_game_t* ui) {
     if (!ui->res.open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2((float)ui->res.x, (float)ui->res.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2((float)ui->res.w, (float)ui->res.h), ImGuiCond_FirstUseEver);
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
                    ui->res.data.poly.offset = 0xffff;
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

static void _ui_game_draw_audio(ui_game_t* ui) {
    if (!ui->audio.open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2((float)ui->audio.x, (float)ui->audio.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2((float)ui->audio.w, (float)ui->audio.h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Audio", &ui->audio.open)) {
        const ImVec2 area = ImGui::GetContentRegionAvail();
        ImGui::PlotLines("##samples", ui->audio.sample_buffer, *ui->audio.num_samples, 0, 0, -1.0f, +1.0f, area);
    }
    ImGui::End();
}

static void _ui_game_draw_video(ui_game_t* ui) {
    if (!ui->video.open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2((float)ui->video.x, (float)ui->video.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2((float)ui->video.w, (float)ui->video.h), ImGuiCond_FirstUseEver);
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
            ImGui::NewLine();
        }
        if (ImGui::CollapsingHeader("Frame buffers", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Current page: %d", ui->game->video.buffers[0]);
            _ui_game_update_fbs(ui);
            for(int i=0; i<4; i++) {
                const ImVec4 border_color = ui->game->video.buffers[0] == i ? ImGui::ColorConvertU32ToFloat4(0xFF30FF30) : ImVec4(0, 0, 0, 1);
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
    ui_game_t* ui = (ui_game_t*) user_data;
    uint8_t* pc = ui->game->res.seg_code;
    *valid = false;
    if (pc != NULL && addr >= 0 && addr < ui->game->res.seg_code_size) {
        *valid = true;
        return pc[addr];
    }
    return 0;
}

static const char* _ui_raw_getstr(uint16_t id, void* user_data) {
    GAME_ASSERT(user_data);
    ui_game_t* ui = (ui_game_t*) user_data;
    return game_get_string(ui->game, id);
}

static uint8_t _ui_raw_dasm_read(int layer, uint16_t addr, bool* valid, void* user_data) {
    GAME_ASSERT(user_data);
    (void)layer;
    ui_game_t* ui = (ui_game_t*) user_data;
    uint8_t* pc = ui->game->res.seg_code;
    *valid = false;
    if (pc != NULL && addr >= 0 && addr < ui->game->res.seg_code_size) {
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
        desc.getstr_cb = _ui_raw_getstr;
        desc.texture_cbs = ui_desc->dbg_texture;
        desc.keys = ui_desc->dbg_keys;
        desc.user_data = ui;
        ui_dbg_init(&ui->dbg, &desc);
    }
    int x = 20, y = 20, dx = 10, dy = 10;
    {
        ui_dasm_desc_t desc = {0};
        desc.layers[0] = "System";
        desc.read_cb = _ui_raw_mem_read;
        desc.getstr_cb = _ui_raw_getstr;
        desc.user_data = ui;
        static const char* titles[4] = { "Disassembler #1", "Disassembler #2", "Disassembler #2", "Dissassembler #3" };
        for (int i = 0; i < 4; i++) {
            desc.title = titles[i]; desc.x = x; desc.y = y;
            ui_dasm_init(&ui->dasm[i], &desc);
            x += dx; y += dy;
        }
    }
    {
        ui_dasm_desc_t desc = {0};
        desc.layers[0] = "System";
        desc.read_cb = _ui_raw_dasm_read;
        desc.getstr_cb = _ui_raw_getstr;
        desc.user_data = ui;
        desc.title = "Disassembler"; desc.x = x; desc.y = y;
        ui_dasm_init(&ui->res.data.code.dasm, &desc);
        x += dx; y += dy;
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
        ui->res.data.poly.pos[0] = 160;
        ui->res.data.poly.pos[1] = 100;
        ui->res.data.poly.zoom = 64;
    }
    {
        ui->vars.x = 10;
        ui->vars.y = 20;
        ui->vars.w = 562;
        ui->vars.h = 568;
    }
    {
        ui_display_desc_t desc = {0};
        desc.title = "Display";
        desc.x = x;
        desc.y = y;
        desc.w = GAME_WIDTH + 20;
        desc.h = GAME_HEIGHT + 20;
        ui_display_init(&ui->display, &desc);
    }
    ui->res.tex_bmp = ui->video.texture_cbs.create_cb(GAME_WIDTH, GAME_HEIGHT);
    ui->res.tex_fb = ui->video.texture_cbs.create_cb(GAME_WIDTH, GAME_HEIGHT);
    ui->audio.sample_buffer = ui->game->audio.sample_buffer;
    ui->audio.num_samples = &ui->game->audio.num_samples;
}

void ui_game_discard(ui_game_t* ui) {
    GAME_ASSERT(ui && ui->game);
    ui->video.texture_cbs.destroy_cb(ui->res.tex_bmp);
    ui->video.texture_cbs.destroy_cb(ui->res.tex_fb);
    for(int i=0; i<4; i++) {
        ui->video.texture_cbs.destroy_cb(ui->video.tex_fb[i]);
    }
    for (int i = 0; i < 4; i++) {
        ui_dasm_discard(&ui->dasm[i]);
    }
    ui_dasm_discard(&ui->res.data.code.dasm);
    ui_dbg_discard(&ui->dbg);
    ui_display_discard(&ui->display);
    ui->game = 0;
}

void ui_game_draw(ui_game_t* ui, const ui_game_frame_t* frame) {
    GAME_ASSERT(ui && ui->game);
    _ui_game_draw_menu(ui);
    _ui_game_draw_resources(ui);
    _ui_game_draw_video(ui);
    _ui_game_draw_vm(ui);
    _ui_game_draw_tasks(ui);
    _ui_game_draw_inputs(ui);
    _ui_game_draw_audio(ui);
    for (int i = 0; i < 4; i++) {
        ui_dasm_draw(&ui->dasm[i]);
    }
    ui_dasm_draw(&ui->res.data.code.dasm);
    ui_dbg_draw(&ui->dbg);
    ui_display_draw(&ui->display, &frame->display);
}

game_debug_t ui_game_get_debug(ui_game_t* ui) {
    GAME_ASSERT(ui);
    game_debug_t res = {};
    res.callback.func = (game_debug_func_t)ui_dbg_tick;
    res.callback.user_data = &ui->dbg;
    res.stopped = &ui->dbg.dbg.stopped;
    return res;
}

void ui_game_save_settings(ui_game_t* ui, ui_settings_t* settings) {
    GAME_ASSERT(ui && settings);
    for (int i = 0; i < 4; i++) {
        ui_dasm_save_settings(&ui->dasm[i], settings);
    }
    ui_dbg_save_settings(&ui->dbg, settings);
    ui_settings_add(settings, "Video", ui->video.open);
    ui_settings_add(settings, "Resources", ui->res.open);
    ui_settings_add(settings, "Variables", ui->vars.open);
    ui_settings_add(settings, "Tasks", ui->tasks.open);
    ui_settings_add(settings, "Inputs", ui->inputs.open);
    ui_settings_add(settings, "Audio", ui->audio.open);
    ui_settings_add(settings, "Display", ui->display.open);
}

void ui_game_load_settings(ui_game_t* ui, const ui_settings_t* settings) {
    GAME_ASSERT(ui && settings);
    for (int i = 0; i < 4; i++) {
        ui_dasm_load_settings(&ui->dasm[i], settings);
    }
    ui_dbg_load_settings(&ui->dbg, settings);
    ui->video.open = ui_settings_isopen(settings, "Video");
    ui->res.open = ui_settings_isopen(settings, "Resources");
    ui->vars.open = ui_settings_isopen(settings, "Variables");
    ui->tasks.open = ui_settings_isopen(settings, "Tasks");
    ui->inputs.open = ui_settings_isopen(settings, "Inputs");
    ui->audio.open = ui_settings_isopen(settings, "Audio");
    ui->display.open = ui_settings_isopen(settings, "Display");
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif /* GAME_UI_IMPL */
