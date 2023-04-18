/*
    RAW.c

    Another world rewrite with sokol.
*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sokol_app.h"
#include "sokol_audio.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "clock.h"
#include "gfx.h"
#include "common.h"
#include "game.h"
#include "raw-data.h"
#if defined(GAME_USE_UI)
    #include "ui.h"
    #include "ui/raw-dasm.h"
    #include "ui/ui_dasm.h"
    #include "ui/ui_dbg.h"
    #include "ui/ui_game.h"
#endif

static struct {
    game_t game;
    uint32_t frame_time_us;
    #ifdef GAME_USE_UI
        ui_game_t ui;
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
#endif

static void app_init(void) {
    game_init(&state.game, &(game_desc_t){
        .part_num = GAME_PART_WATER,
        .demo3_joy = dump_DEMO3_JOY,
        .demo3_joy_size = sizeof(dump_DEMO3_JOY),
        .lang = GAME_LANG_US,
        .data = {
            .mem_list = dump_MEMLIST_BIN,
            .banks[0x0] = dump_BANK01,
            .banks[0x1] = dump_BANK02,
            .banks[0x4] = dump_BANK05,
            .banks[0x5] = dump_BANK06,
            .banks[0xc] = dump_BANK0D,
        },
        .audio = {
            .callback = { .func = push_audio },
            .sample_rate = saudio_sample_rate()
        },
         #if defined(GAME_USE_UI)
            .debug = ui_game_get_debug(&state.ui)
        #endif
    });
    sapp_set_window_title(state.game.title);
    clock_init();
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
        });
    #endif
}

static void app_frame(void) {
    state.frame_time_us = clock_frame_time();
    gfx_draw(game_display_info(&state.game));
    game_exec(&state.game, state.frame_time_us/1000);
}

static void app_cleanup(void) {
    game_cleanup(&state.game);
    #ifdef GAME_USE_UI
        ui_game_discard(&state.ui);
        ui_discard();
    #endif
    saudio_shutdown();
    gfx_shutdown();
}

void app_input(const sapp_event* event) {
#ifdef GAME_USE_UI
    if (ui_input(event)) {
        // input was handled by UI
        return;
    }
#endif
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

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc, (void)argv;
    return (sapp_desc) {
        .init_cb = app_init,
        .event_cb = app_input,
        .frame_cb = app_frame,
        .cleanup_cb = app_cleanup,
        .width = 800,
        .height = 600,
        .window_title = "RAW",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
