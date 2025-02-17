#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_gl.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "gfx.h"
#include "shaders.glsl.h"
#include <assert.h>
#include <stdlib.h> // malloc/free
#include <string.h> // memset

#define GFX_DEF(v,def) (v?v:def)

typedef struct {
    bool valid;
    gfx_border_t border;
    struct {
        sg_image img;       // framebuffer texture, RGBA8 or R8 if paletted
        sg_image pal_img;   // optional color palette texture
        sg_sampler smp;
        gfx_dim_t dim;
    } fb;
    struct {
        gfx_rect_t view;
        gfx_dim_t pixel_aspect;
        sg_image img;
        sg_sampler smp;
        sg_buffer vbuf;
        sg_pipeline pip;
        sg_attachments attachments;
        sg_pass_action pass_action;
    } offscreen;
    struct {
        sg_buffer vbuf;
        sg_pipeline pip;
        sg_pass_action pass_action;
        bool portrait;
    } display;
    int flash_success_count;
    int flash_error_count;
    sg_image empty_snapshot_texture;
    void (*draw_extra_cb)(const gfx_draw_info_t* draw_info);
} gfx_state_t;
static gfx_state_t state;

static const float gfx_verts[] = {
    0.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f
};
static const float gfx_verts_rot[] = {
    0.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f, 1.0f
};
static const float gfx_verts_flipped[] = {
    0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 1.0f, 0.0f
};
static const float gfx_verts_flipped_rot[] = {
    0.0f, 0.0f, 1.0f, 1.0f,
    1.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f, 0.0f
};

gfx_dim_t gfx_pixel_aspect(void) {
    assert(state.valid);
    return state.offscreen.pixel_aspect;
}

sg_image gfx_create_icon_texture(const uint8_t* packed_pixels, int width, int height, int stride) {
    // textures must be 2^n for WebGL
    const size_t pixel_data_size = width * height * sizeof(uint32_t);
    uint32_t* pixels = malloc(pixel_data_size);
    assert(pixels);
    memset(pixels, 0, pixel_data_size);
    const uint8_t* src = packed_pixels;
    uint32_t* dst = pixels;
    for (int y = 0; y < height; y++) {
        uint8_t bits = 0;
        dst = pixels + (y * width);
        for (int x = 0; x < width; x++) {
            if ((x & 7) == 0) {
                bits = *src++;
            }
            if (bits & 1) {
                *dst++ = 0xFFFFFFFF;
            }
            else {
                *dst++ = 0x00FFFFFF;
            }
            bits >>= 1;
        }
    }
    assert(src == packed_pixels + stride * height); (void)stride;   // stride is unused in release mode
    assert(dst <= pixels + (width * height));
    sg_image img = sg_make_image(&(sg_image_desc){
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .width = width,
        .height = height,
        .data.subimage[0][0] = { .ptr=pixels, .size=pixel_data_size }
    });
    free(pixels);
    return img;
}

// this function will be called at init time and when the emulator framebuffer size changes
static void gfx_init_images_and_pass(void) {
    // destroy previous resources (if exist)
    sg_destroy_image(state.fb.img);
    sg_destroy_sampler(state.fb.smp);
    sg_destroy_image(state.offscreen.img);
    sg_destroy_sampler(state.offscreen.smp);
    sg_destroy_attachments(state.offscreen.attachments);

    // a texture with the emulator's raw pixel data
    assert((state.fb.dim.width > 0) && (state.fb.dim.height > 0));
    state.fb.img = sg_make_image(&(sg_image_desc){
        .width = state.fb.dim.width,
        .height = state.fb.dim.height,
        .pixel_format = SG_PIXELFORMAT_R8,
        .usage = SG_USAGE_STREAM,
    });

    // a sampler for sampling the emulators raw pixel data
    state.fb.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });

    // 2x-upscaling render target texture, sampler and pass
    assert((state.offscreen.view.width > 0) && (state.offscreen.view.height > 0));
    state.offscreen.img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 2 * state.offscreen.view.width,
        .height = 2 * state.offscreen.view.height,
        .sample_count = 1,
    });
    state.offscreen.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });
    state.offscreen.attachments = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = state.offscreen.img
    });
}

static const struct {
    int width;
    int height;
    int stride;
    uint8_t pixels[32];
} empty_snapshot_icon = {
    .width = 16,
    .height = 16,
    .stride = 2,
    .pixels = {
        0xFF,0xFF,
        0x03,0xC0,
        0x05,0xA0,
        0x09,0x90,
        0x11,0x88,
        0x21,0x84,
        0x41,0x82,
        0x81,0x81,
        0x81,0x81,
        0x41,0x82,
        0x21,0x84,
        0x11,0x88,
        0x09,0x90,
        0x05,0xA0,
        0x03,0xC0,
        0xFF,0xFF,

    }
};

void gfx_init(const gfx_desc_t* desc) {
    sg_setup(&(sg_desc){
        .buffer_pool_size = 32,
        .image_pool_size = 128,
        .shader_pool_size = 16,
        .pipeline_pool_size = 16,
        .attachments_pool_size = 2,
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    sgl_setup(&(sgl_desc_t){
        .max_vertices = 16,
        .max_commands = 16,
        .context_pool_size = 1,
        .pipeline_pool_size = 16,
        .logger.func = slog_func,
    });

    state.valid = true;
    state.display.portrait = desc->display_info.portrait;
    state.draw_extra_cb = desc->draw_extra_cb;
    state.fb.dim = desc->display_info.frame.dim;
    state.offscreen.pixel_aspect.width = GFX_DEF(desc->pixel_aspect.width, 1);
    state.offscreen.pixel_aspect.height = GFX_DEF(desc->pixel_aspect.height, 1);
    state.offscreen.view = desc->display_info.screen;

    static uint32_t palette_buf[256];
    assert((desc->display_info.palette.size > 0) && (desc->display_info.palette.size <= sizeof(palette_buf)));
    memcpy(palette_buf, desc->display_info.palette.ptr, desc->display_info.palette.size);
    state.fb.pal_img = sg_make_image(&(sg_image_desc){
        .width = 256,
        .height = 1,
        .usage = SG_USAGE_STREAM,
        .pixel_format = SG_PIXELFORMAT_RGBA8
    });

    state.offscreen.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_DONTCARE }
    };
    state.offscreen.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(gfx_verts)
    });

    sg_shader shd = sg_make_shader(offscreen_pal_shader_desc(sg_query_backend()));
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT2,
                [1].format = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .depth.pixel_format = SG_PIXELFORMAT_NONE
    });

    state.display.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.05f, 0.05f, 0.05f, 1.0f } }
    };
    state.display.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = {
            .ptr = sg_query_features().origin_top_left ?
                   (state.display.portrait ? gfx_verts_rot : gfx_verts) :
                   (state.display.portrait ? gfx_verts_flipped_rot : gfx_verts_flipped),
            .size = sizeof(gfx_verts)
        }
    });

    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT2,
                [1].format = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP
    });

    // create an icon texture for an empty snapshot
    state.empty_snapshot_texture = gfx_create_icon_texture(
        empty_snapshot_icon.pixels,
        empty_snapshot_icon.width,
        empty_snapshot_icon.height,
        empty_snapshot_icon.stride);

    // create image and pass resources
    gfx_init_images_and_pass();
}

/* apply a viewport rectangle to preserve the emulator's aspect ratio,
   and for 'portrait' orientations, keep the emulator display at the
   top, to make room at the bottom for mobile virtual keyboard
*/
static void apply_viewport(gfx_dim_t canvas, gfx_rect_t view, gfx_dim_t pixel_aspect, gfx_border_t border) {
    float cw = (float) (canvas.width - border.left - border.right);
    if (cw < 1.0f) {
        cw = 1.0f;
    }
    float ch = (float) (canvas.height - border.top - border.bottom);
    if (ch < 1.0f) {
        ch = 1.0f;
    }
    const float canvas_aspect = (float)cw / (float)ch;
    const gfx_dim_t aspect = pixel_aspect;
    const float emu_aspect = (float)(view.width * aspect.width) / (float)(view.height * aspect.height);
    float vp_x, vp_y, vp_w, vp_h;
    if (emu_aspect < canvas_aspect) {
        vp_y = (float)border.top;
        vp_h = ch;
        vp_w = (ch * emu_aspect);
        vp_x = border.left + (cw - vp_w) / 2;
    }
    else {
        vp_x = (float)border.left;
        vp_w = cw;
        vp_h = (cw / emu_aspect);
        vp_y = (float)border.top;
    }
    sg_apply_viewportf(vp_x, vp_y, vp_w, vp_h, true);
}

void gfx_draw(gfx_display_info_t display_info) {
    assert(state.valid);
    assert((display_info.frame.dim.width > 0) && (display_info.frame.dim.height > 0));
    assert(display_info.frame.buffer.ptr && (display_info.frame.buffer.size > 0));
    assert((display_info.screen.width > 0) && (display_info.screen.height > 0));
    const gfx_dim_t display = { .width = sapp_width(), .height = sapp_height() };

    state.offscreen.view = display_info.screen;

    // check if emulator framebuffer size has changed, need to create new backing texture
    if ((display_info.frame.dim.width != state.fb.dim.width) || (display_info.frame.dim.height != state.fb.dim.height)) {
        state.fb.dim = display_info.frame.dim;
        gfx_init_images_and_pass();
    }

    // copy emulator pixel data into emulator framebuffer texture
    sg_update_image(state.fb.img, &(sg_image_data){
        .subimage[0][0] = {
            .ptr = display_info.frame.buffer.ptr,
            .size = display_info.frame.buffer.size,
        }
    });

    sg_update_image(state.fb.pal_img, &(sg_image_data){
        .subimage[0][0] = {
            .ptr = display_info.palette.ptr,
            .size = display_info.palette.size,
        }
    });

    // upscale the original framebuffer 2x with nearest filtering
    sg_begin_pass(&(sg_pass){
        .action = state.offscreen.pass_action,
        .attachments = state.offscreen.attachments
    });
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.offscreen.vbuf,
        .images = {
            [IMG_fb_tex] = state.fb.img,
            [IMG_pal_tex] = state.fb.pal_img,
        },
        .samplers[SMP_smp] = state.fb.smp,
    });
    const offscreen_vs_params_t vs_params = {
        .uv_offset = {
            (float)state.offscreen.view.x / (float)state.fb.dim.width,
            (float)state.offscreen.view.y / (float)state.fb.dim.height,
        },
        .uv_scale = {
            (float)state.offscreen.view.width / (float)state.fb.dim.width,
            (float)state.offscreen.view.height / (float)state.fb.dim.height
        }
    };
    sg_apply_uniforms(UB_offscreen_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 4, 1);
    sg_end_pass();

    // tint the clear color red or green if flash feedback is requested
    if (state.flash_error_count > 0) {
        state.flash_error_count--;
        state.display.pass_action.colors[0].clear_value.r = 0.7f;
    }
    else if (state.flash_success_count > 0) {
        state.flash_success_count--;
        state.display.pass_action.colors[0].clear_value.g = 0.7f;
    }
    else {
        state.display.pass_action.colors[0].clear_value.r = 0.05f;
        state.display.pass_action.colors[0].clear_value.g = 0.05f;
    }

    // draw the final pass with linear filtering
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain()
    });
    apply_viewport(display, display_info.screen, state.offscreen.pixel_aspect, state.border);
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.display.vbuf,
        .images[IMG_tex] = state.offscreen.img,
        .samplers[SMP_smp] = state.offscreen.smp,
    });
    sg_draw(0, 4, 1);
    sg_apply_viewport(0, 0, display.width, display.height, true);
    sgl_draw();
    if (state.draw_extra_cb) {
        state.draw_extra_cb(&(gfx_draw_info_t){
            .display_image = state.offscreen.img,
            .display_sampler = state.offscreen.smp,
            .display_info = display_info,
        });
    }
    sg_end_pass();
    sg_commit();
}

void gfx_shutdown() {
    assert(state.valid);
    sgl_shutdown();
    sg_shutdown();
}

void gfx_flash_success(void) {
    assert(state.valid);
    state.flash_success_count = 20;
}

void gfx_flash_error(void) {
    assert(state.valid);
    state.flash_error_count = 20;
}
