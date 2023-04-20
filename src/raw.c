/*
    RAW.c

    Another world rewrite with sokol.
*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sokol_app.h"
#include "sokol_args.h"
#include "sokol_audio.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "clock.h"
#include "gfx.h"
#include "fs.h"
#include "common.h"
#include "game.h"
#include "raw-data.h"
#include "miniz.h"
#if defined(GAME_USE_UI)
    #include "ui.h"
    #include "ui/raw-dasm.h"
    #include "ui/ui_dasm.h"
    #include "ui/ui_dbg.h"
    #include "ui/ui_snapshot.h"
    #include "ui/ui_game.h"
#endif

typedef struct {
    uint32_t    version;
    game_t      game;
} game_snapshot_t;

static struct {
    bool            ready;
    game_data_t     data;
    game_t          game;
    uint32_t        frame_time_us;
    #ifdef GAME_USE_UI
        ui_game_t   ui;
        game_snapshot_t snapshots[UI_SNAPSHOT_MAX_SLOTS];
    #endif
} state;

#ifdef GAME_USE_UI
static void ui_draw_cb(void);
#endif

// audio-streaming callback
static void push_audio(const float* samples, int num_samples, void* user_data) {
    (void)user_data;
    saudio_push(samples, num_samples/2);
}

#if defined(GAME_USE_UI)
static void ui_draw_cb(void) {
    ui_game_draw(&state.ui);
}

static void ui_update_snapshot_screenshot(size_t slot) {
    ui_snapshot_screenshot_t screenshot = {
        .texture = gfx_create_screenshot_texture(game_display_info(&state.snapshots[slot].game))
    };
    ui_snapshot_screenshot_t prev_screenshot = ui_snapshot_set_screenshot(&state.ui.snapshot, slot, screenshot);
    if (prev_screenshot.texture) {
        gfx_destroy_texture(prev_screenshot.texture);
    }
}

static bool ui_load_snapshot(size_t slot) {
    bool success = false;
    if ((slot < UI_SNAPSHOT_MAX_SLOTS) && (state.ui.snapshot.slots[slot].valid)) {
        success = game_load_snapshot(&state.game, state.snapshots[slot].version, &state.snapshots[slot].game);
    }
    return success;
}

static void ui_save_snapshot(size_t slot) {
    if (slot < UI_SNAPSHOT_MAX_SLOTS) {
        state.snapshots[slot].version = game_save_snapshot(&state.game, &state.snapshots[slot].game);
        ui_update_snapshot_screenshot(slot);
        fs_save_snapshot("raw", slot, (gfx_range_t){ .ptr = &state.snapshots[slot], sizeof(gfx_range_t) });
    }
}
#endif

static int _to_num(char c) {
    if(c > '0' && c < '9') {
        return c - '0';
    } else if(c > 'a') {
        return 10 + c - 'a';
    } else if(c > 'A') {
        return 10 + c - 'A';
    }
    return 0;
}

static void app_init(void) {
    clock_init();
    fs_init();
    game_init(&state.game, &(game_desc_t){
        .part_num = GAME_PART_INTRO,
// TODO:
//        .demo3_joy = dump_DEMO3_JOY,
//        .demo3_joy_size = sizeof(dump_DEMO3_JOY),
        .lang = GAME_LANG_US,
        .audio = {
            .callback = { .func = push_audio },
            .sample_rate = saudio_sample_rate()
        },
         #if defined(GAME_USE_UI)
            .debug = ui_game_get_debug(&state.ui)
        #endif
    });
    gfx_init(&(gfx_desc_t){
        .display_info = game_display_info(&state.game),
        #ifdef GAME_USE_UI
            .draw_extra_cb = ui_draw,
        #endif
    });
    saudio_setup(&(saudio_desc){
        .num_channels = 2,
        .logger.func = slog_func,
    });
    #ifdef GAME_USE_UI
        ui_init(ui_draw_cb);
        ui_game_init(&state.ui, &(ui_game_desc_t){
            .game = &state.game,
            .dbg_texture = {
                .create_cb = gfx_create_texture,
                .update_cb = gfx_update_texture,
                .destroy_cb = gfx_destroy_texture,
            },
            .snapshot = {
                .load_cb = ui_load_snapshot,
                .save_cb = ui_save_snapshot,
                .empty_slot_screenshot = {
                    .texture = gfx_shared_empty_snapshot_texture(),
                }
            }
        });
    #endif

    if (sargs_exists("file")) {
        fs_start_load_file(FS_SLOT_IMAGE, sargs_value("file"));
    }
}

static void _game_start(void) {
    game_start(&state.game, state.data);
    sapp_set_window_title(state.game.title);
}

bool _game_load_data(gfx_range_t data) {
    mz_zip_archive archive;
    mz_zip_zero_struct(&archive);
    mz_zip_reader_init_mem(&archive, data.ptr, data.size, 0);
    mz_uint num = mz_zip_reader_get_num_files(&archive);
    mz_zip_archive_file_stat stat;
    bool result = false;
    for(mz_uint i=0; i<num; i++) {
        mz_zip_reader_file_stat(&archive, i, &stat);
        if(strncmp(stat.m_filename, "memlist.bin", 11) == 0) {
            result = true;
            void* ptr = malloc(stat.m_uncomp_size);
            mz_zip_reader_extract_to_mem(&archive, i, ptr, stat.m_uncomp_size, 0);
            state.data.mem_list = ptr;
        } else if(strncmp(stat.m_filename, "bank", 4) == 0) {
            result = true;
            int bank_n = _to_num(stat.m_filename[5]);
            void* ptr = malloc(stat.m_uncomp_size);
            mz_zip_reader_extract_to_mem(&archive, i, ptr, stat.m_uncomp_size, 0);
            state.data.banks[bank_n - 1] = ptr;
        }
    }
    mz_zip_reader_end(&archive);
    return result;
}

static void handle_file_loading(void) {
    fs_dowork();
    const uint32_t load_delay_frames = 120;
    if (fs_success(FS_SLOT_IMAGE) && clock_frame_count_60hz() > load_delay_frames) {

        bool load_success = false;
        if (fs_ext(FS_SLOT_IMAGE, "zip")) {
            load_success = _game_load_data(fs_data(FS_SLOT_IMAGE));
        }
        if (load_success) {
            state.ready = true;
            _game_start();
            if (clock_frame_count_60hz() > (load_delay_frames + 10)) {
                gfx_flash_success();
            }
        }
        else {
            gfx_flash_error();
        }
        fs_reset(FS_SLOT_IMAGE);
    }
}

static void app_frame(void) {
    state.frame_time_us = clock_frame_time();
    gfx_draw(game_display_info(&state.game));
    if(state.ready) {
        game_exec(&state.game, state.frame_time_us/1000);
    }
    handle_file_loading();
}

static void app_cleanup(void) {
    game_cleanup(&state.game);
    #ifdef GAME_USE_UI
        ui_game_discard(&state.ui);
        ui_discard();
    #endif
    saudio_shutdown();
    gfx_shutdown();
    sargs_shutdown();
}

void app_input(const sapp_event* event) {
     // accept dropped files also when ImGui grabs input
    if (event->type == SAPP_EVENTTYPE_FILES_DROPPED) {
        fs_start_load_dropped_file(FS_SLOT_IMAGE);
    }
#ifdef GAME_USE_UI
    if (ui_input(event)) {
        // input was handled by UI
        return;
    }
#endif
    if(state.ready) {
        switch (event->type) {
            case SAPP_EVENTTYPE_CHAR: {
                int c = (int)event->char_code;
                if ((c > 0x20) && (c < 0x7F)) {
                    game_char_pressed(&state.game, c);
                }
            }
            break;
            case SAPP_EVENTTYPE_KEY_DOWN:
            case SAPP_EVENTTYPE_KEY_UP: {
                game_input_t c;
                switch (event->key_code) {
                    case SAPP_KEYCODE_LEFT:         c = GAME_INPUT_LEFT; break;
                    case SAPP_KEYCODE_RIGHT:        c = GAME_INPUT_RIGHT; break;
                    case SAPP_KEYCODE_DOWN:         c = GAME_INPUT_DOWN; break;
                    case SAPP_KEYCODE_UP:           c = GAME_INPUT_UP; break;
                    case SAPP_KEYCODE_ENTER:        c = GAME_INPUT_ACTION; break;
                    case SAPP_KEYCODE_SPACE:        c = GAME_INPUT_ACTION; break;
                    case SAPP_KEYCODE_ESCAPE:       c = GAME_INPUT_BACK; break;
                    case SAPP_KEYCODE_F:            c = GAME_INPUT_BACK; break;
                    case SAPP_KEYCODE_C:            c = GAME_INPUT_CODE; break;
                    case SAPP_KEYCODE_P:            c = GAME_INPUT_PAUSE; break;
                    default:                        c = -1; break;
                }
                if ((int)c != -1) {
                    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
                        game_key_down(&state.game, c);
                    }
                    else {
                        game_key_up(&state.game, c);
                    }
                }
                break;
            default:
                break;
            }
        }
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    sargs_setup(&(sargs_desc){ .argc=argc, .argv=argv });
    return (sapp_desc) {
        .init_cb = app_init,
        .event_cb = app_input,
        .frame_cb = app_frame,
        .cleanup_cb = app_cleanup,
        .width = 800,
        .height = 600,
        .window_title = "RAW",
        .icon.sokol_default = true,
        .enable_dragndrop = true,
        .logger.func = slog_func,
    };
}
