#
# project: raw
#
cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
project(raw)
# include the fips main cmake file
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")

include_directories(src)
include_directories(libs/gfx)
include_directories(libs/sokol)
fips_setup()
fips_import_fips_imgui_imgui()

#  platform selection
add_definitions(-DSOKOL_NO_DEPRECATED)
if (FIPS_EMSCRIPTEN)
    if (FIPS_EMSCRIPTEN_USE_WEBGPU)
        set(sokol_backend SOKOL_WGPU)
        set(slang "wgpu")
    else()
        set(sokol_backend SOKOL_GLES3)
        set(slang "glsl300es")
    endif()
elseif (FIPS_ANDROID)
    set(sokol_backend SOKOL_GLES3)
    set(slang "glsl300es")
elseif (SOKOL_USE_D3D11)
    set(sokol_backend SOKOL_D3D11)
    set(slang "hlsl4")
elseif (SOKOL_USE_METAL)
    set(sokol_backend SOKOL_METAL)
    if (FIPS_IOS)
        set(slang "metal_ios:metal_sim")
    else()
        set(slang "metal_macos")
    endif()
elseif (SOKOL_USE_EGL_GLES3)
    set(sokol_backend SOKOL_GLES3)
    set(slang "glsl300es")
elseif (SOKOL_USE_EGL_GL)
    set(sokol_backend SOKOL_GLCORE33)
    set(slang "glsl330")
else()
    if (FIPS_IOS)
        set(sokol_backend SOKOL_GLES3)
        set(slang "glsl300es")
    else()
        set(sokol_backend SOKOL_GLCORE33)
        set(slang "glsl330")
    endif()
endif()

fips_ide_group("Libs")
add_subdirectory(libs)
fips_ide_group("Src")
add_subdirectory(src)
