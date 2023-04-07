/*
    RAW.c

    Another world rewrite with sokol.
*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "clock.h"
#include "gfx.h"
#include "game.h"

static struct {
    game_t game;
    uint32_t frame_time_us;
} state;

static void app_init(void) {
    game_init(&state.game, &(game_desc_t){
        .part_num = 16001
    });
    clock_init();
    gfx_init(&(gfx_desc_t){
        .display_info = game_display_info(&state.game),
    });
    sapp_set_window_title(state.game.title);
}

static void app_frame(void) {
    state.frame_time_us = clock_frame_time();
    gfx_draw(game_display_info(&state.game));
    game_exec(&state.game, state.frame_time_us/1000);
}

static void app_cleanup(void) {
    game_cleanup(&state.game);
    gfx_shutdown();
}

void app_input(const sapp_event* event) {
    switch (event->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
        case SAPP_EVENTTYPE_KEY_UP: {
            game_input_t c;
            switch (event->key_code) {
                case SAPP_KEYCODE_LEFT:         c = GAME_INPUT_LEFT; break;
                case SAPP_KEYCODE_RIGHT:        c = GAME_INPUT_RIGHT; break;
                case SAPP_KEYCODE_DOWN:         c = GAME_INPUT_DOWN; break;
                case SAPP_KEYCODE_UP:           c = GAME_INPUT_UP; break;
                case SAPP_KEYCODE_D:            c = GAME_INPUT_JUMP; break;
                case SAPP_KEYCODE_S:            c = GAME_INPUT_ACTION; break;
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
