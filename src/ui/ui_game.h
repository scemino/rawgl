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

    Include the following headers (and their depenencies) before including
    ui_1013.h both for the declaration and implementation.

    - game.h

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

/* a callback to create a dynamic-update RGBA8 UI texture, needs to return an ImTextureID handle */
typedef void* (*ui_dbg_create_texture_t)(int w, int h);
/* callback to update a UI texture with new data */
typedef void (*ui_dbg_update_texture_t)(void* tex_handle, void* data, int data_byte_size);
/* callback to destroy a UI texture */
typedef void (*ui_dbg_destroy_texture_t)(void* tex_handle);

typedef struct ui_dbg_texture_callbacks_t {
    ui_dbg_create_texture_t create_cb;      // callback to create UI texture
    ui_dbg_update_texture_t update_cb;      // callback to update UI texture
    ui_dbg_destroy_texture_t destroy_cb;    // callback to destroy UI texture
} ui_dbg_texture_callbacks_t;

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
    game_resource_t res;
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
} ui_game_t;

void ui_game_init(ui_game_t* ui, const ui_game_desc_t* desc);
void ui_game_discard(ui_game_t* ui);
void ui_game_draw(ui_game_t* ui);

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
                game_mem_entry_t* e = &ui->res.res.mem_entries[i];
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

void ui_game_init(ui_game_t* ui, const ui_game_desc_t* ui_desc) {
    GAME_ASSERT(ui && ui_desc);
    GAME_ASSERT(ui_desc->game);
    ui->game = ui_desc->game;
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
}

void ui_game_discard(ui_game_t* ui) {
    GAME_ASSERT(ui && ui->game);
    for(int i=0; i<4; i++) {
        ui->video.texture_cbs.destroy_cb(ui->video.tex_fb[i]);
    }
    ui->game = 0;
}

void ui_game_draw(ui_game_t* ui) {
    GAME_ASSERT(ui && ui->game);
    _ui_game_draw_menu(ui);
    _ui_game_draw_resources(ui);
    _ui_game_draw_video(ui);
    _ui_game_draw_script(ui);
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif /* GAME_UI_IMPL */
