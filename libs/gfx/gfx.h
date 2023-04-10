#pragma once
/*
    Common graphics functions for the chips-test example emulators.

    REMINDER: consider using this CRT shader?

    https://github.com/mattiasgustavsson/rebasic/blob/master/source/libs/crtemu.h
*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int top, bottom, left, right;
} gfx_border_t;

typedef struct {
    void* ptr;
    size_t size;
} gfx_range_t;

typedef struct {
    int width, height;
} gfx_dim_t;

typedef struct {
    int x, y, width, height;
} gfx_rect_t;

typedef struct {
    struct {
        gfx_dim_t dim;        // framebuffer dimensions in pixels
        gfx_range_t buffer;
        size_t bytes_per_pixel; // 1 or 4
    } frame;
    gfx_rect_t screen;
    gfx_range_t palette;
    bool portrait;
} gfx_display_info_t;

typedef struct {
    gfx_display_info_t display_info;
    gfx_dim_t pixel_aspect;   // optional pixel aspect ratio, default is 1:1
    void (*draw_extra_cb)(void);
} gfx_desc_t;

void gfx_init(const gfx_desc_t* desc);
void gfx_draw(gfx_display_info_t display_info);
void gfx_shutdown(void);

// UI helper functions unrelated to actual emulator framebuffer rendering
void* gfx_create_texture(int w, int h);
void gfx_update_texture(void* h, void* data, int data_byte_size);
void gfx_destroy_texture(void* h);
void* gfx_create_screenshot_texture(gfx_display_info_t display_info);

#ifdef __cplusplus
} /* extern "C" */
#endif
