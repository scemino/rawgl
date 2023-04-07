#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_gl.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "gfx.h"
#include "shaders.glsl.h"
#include <assert.h>
#include <stdlib.h> // malloc/free

#define GFX_DEF(v,def) (v?v:def)

#define GFX_DELETE_STACK_SIZE (32)

typedef struct {
    bool valid;
    gfx_border_t border;
    struct {
        sg_image img;       // framebuffer texture, R8 if paletted
        sg_image pal_img;   // palette texture
        gfx_dim_t dim;
    } fb;
    struct {
        gfx_rect_t view;
        gfx_dim_t pixel_aspect;
        sg_image img;
        sg_buffer vbuf;
        sg_pipeline pip;
        sg_pass pass;
        sg_pass_action pass_action;
    } offscreen;
    struct {
        sg_buffer vbuf;
        sg_pipeline pip;
        sg_pass_action pass_action;
        bool portrait;
    } display;
    struct {
        sg_image img;
        sgl_pipeline pip;
        gfx_dim_t dim;
    } icon;
    struct {
        sg_image images[GFX_DELETE_STACK_SIZE];
        size_t cur_slot;
    } delete_stack;
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

// this function will be called at init time and when the emulator framebuffer size changes
static void gfx_init_images_and_pass(void) {
    // destroy previous resources (if exist)
    sg_destroy_image(state.fb.img);
    sg_destroy_image(state.offscreen.img);
    sg_destroy_pass(state.offscreen.pass);

    // a texture with the emulator's raw pixel data
    assert((state.fb.dim.width > 0) && (state.fb.dim.height > 0));
    state.fb.img = sg_make_image(&(sg_image_desc){
        .width = state.fb.dim.width,
        .height = state.fb.dim.height,
        .pixel_format = SG_PIXELFORMAT_R8,
        .usage = SG_USAGE_STREAM,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });

    // 2x-upscaling render target textures and passes
    assert((state.offscreen.view.width > 0) && (state.offscreen.view.height > 0));
    state.offscreen.img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 2 * state.offscreen.view.width,
        .height = 2 * state.offscreen.view.height,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .sample_count = 1,
    });
    state.offscreen.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = state.offscreen.img
    });
}

void gfx_init(const gfx_desc_t* desc) {
    sg_setup(&(sg_desc){
        .buffer_pool_size = 32,
        .image_pool_size = 128,
        .shader_pool_size = 16,
        .pipeline_pool_size = 16,
        .context_pool_size = 2,
        .context = sapp_sgcontext(),
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
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });

    state.offscreen.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_DONTCARE }
    };
    state.offscreen.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(gfx_verts)
    });

    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(offscreen_pal_shader_desc(sg_query_backend())),
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
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.05f, 0.05f, 0.05f, 1.0f } }
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
    sg_begin_pass(state.offscreen.pass, &state.offscreen.pass_action);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.offscreen.vbuf,
        .fs_images[SLOT_fb_tex] = state.fb.img,
        .fs_images[SLOT_pal_tex] = state.fb.pal_img,
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
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_offscreen_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 4, 1);
    sg_end_pass();

    // draw the final pass with linear filtering
    sg_begin_default_pass(&state.display.pass_action, display.width, display.height);
    apply_viewport(display, display_info.screen, state.offscreen.pixel_aspect, state.border);
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.display.vbuf,
        .fs_images[SLOT_tex] = state.offscreen.img,
    });
    sg_draw(0, 4, 1);
    sg_apply_viewport(0, 0, display.width, display.height, true);
    sgl_draw();
    sg_end_pass();
    sg_commit();

    // garbage collect images
    for (size_t i = 0; i < state.delete_stack.cur_slot; i++) {
        sg_destroy_image(state.delete_stack.images[i]);
    }
    state.delete_stack.cur_slot = 0;
}

void gfx_shutdown() {
    assert(state.valid);
    sgl_shutdown();
    sg_shutdown();
}

void* gfx_create_texture(int w, int h) {
    sg_image img = sg_make_image(&(sg_image_desc){
        .width = w,
        .height = h,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });
    return (void*)(uintptr_t)img.id;
}

// creates a 2x downscaled screenshot texture of the emulator framebuffer
void* gfx_create_screenshot_texture(gfx_display_info_t info) {
    assert(info.frame.buffer.ptr);

    size_t dst_w = (info.screen.width + 1) >> 1;
    size_t dst_h = (info.screen.height + 1) >> 1;
    size_t dst_num_bytes = (size_t)(dst_w * dst_h * 4);
    uint32_t* dst = calloc(1, dst_num_bytes);

    if (info.palette.ptr) {
        assert(info.frame.bytes_per_pixel == 1);
        const uint8_t* pixels = (uint8_t*) info.frame.buffer.ptr;
        const uint32_t* palette = (uint32_t*) info.palette.ptr;
        const size_t num_palette_entries = info.palette.size / sizeof(uint32_t);
        for (size_t y = 0; y < (size_t)info.screen.height; y++) {
            for (size_t x = 0; x < (size_t)info.screen.width; x++) {
                uint8_t p = pixels[(y + info.screen.y) * info.frame.dim.width + (x + info.screen.x)];
                assert(p < num_palette_entries); (void)num_palette_entries;
                uint32_t c = (palette[p] >> 2) & 0x3F3F3F3F;
                size_t dst_x = x >> 1;
                size_t dst_y = y >> 1;
                if (info.portrait) {
                    dst[dst_x * dst_h + (dst_h - dst_y - 1)] += c;
                }
                else {
                    dst[dst_y * dst_w + dst_x] += c;
                }
            }
        }
    }
    else {
        assert(info.frame.bytes_per_pixel == 4);
        const uint32_t* pixels = (uint32_t*) info.frame.buffer.ptr;
        for (size_t y = 0; y < (size_t)info.screen.height; y++) {
            for (size_t x = 0; x < (size_t)info.screen.width; x++) {
                uint32_t c = pixels[(y + info.screen.y) * info.frame.dim.width + (x + info.screen.x)];
                c = (c >> 2) & 0x3F3F3F3F;
                size_t dst_x = x >> 1;
                size_t dst_y = y >> 1;
                if (info.portrait) {
                    dst[dst_x * dst_h + (dst_h - dst_y - 1)] += c;
                }
                else {
                    dst[dst_y * dst_w + dst_x] += c;
                }
            }
        }
    }

    sg_image img = sg_make_image(&(sg_image_desc){
        .width = (int) (info.portrait ? dst_h : dst_w),
        .height = (int) (info.portrait ? dst_w : dst_h),
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .data.subimage[0][0] = {
            .ptr = dst,
            .size = dst_num_bytes,
        }
    });
    free(dst);
    return (void*)(uintptr_t)img.id;
}

void gfx_update_texture(void* h, void* data, int data_byte_size) {
    sg_image img = { .id=(uint32_t)(uintptr_t)h };
    sg_update_image(img, &(sg_image_data){.subimage[0][0] = { .ptr = data, .size=data_byte_size } });
}

void gfx_destroy_texture(void* h) {
    sg_image img = { .id=(uint32_t)(uintptr_t)h };
    if (state.delete_stack.cur_slot < GFX_DELETE_STACK_SIZE) {
        state.delete_stack.images[state.delete_stack.cur_slot++] = img;
    }
}

