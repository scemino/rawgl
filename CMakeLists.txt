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
fips_import_dcimgui_dcimgui()

fips_ide_group("Libs")
add_subdirectory(libs)
fips_ide_group("Src")
add_subdirectory(src)
