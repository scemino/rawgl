fips_begin_lib(gfx)
    fips_files(gfx.h gfx.c)
    sokol_shader(shaders.glsl ${slang})
fips_end_lib()
