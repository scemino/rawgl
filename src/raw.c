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
#include "gfx.h"
#include "game.h"

static struct {
    game_t game;
    uint32_t frame_time_us;
    uint32_t ticks;
    double emu_time_ms;
} state;

static void app_init(void) {
    game_init(&state.game, &(game_desc_t){
        .part_num = 16001,
        .bypass_protection = true,
        .demo3_joy_inputs = true
    });
    gfx_init(&(gfx_desc_t){
        .display_info = game_display_info(&state.game),
    });
    sapp_set_window_title(state.game.title);
}

static void app_frame(void) {
    gfx_draw(game_display_info(&state.game));
    game_exec(&state.game);
}

static void app_cleanup(void) {
    gfx_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc, (void)argv;
    return (sapp_desc) {
        .init_cb = app_init,
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
