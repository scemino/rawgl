fips_begin_app(raw windowed)
    fips_files(raw.c game-impl.cc
    aifcplayer.cpp bitmap.cpp file.cpp graphics_sokol.cc sokol_mixer.cc pak.cpp resource.cpp resource_3do.cpp resource_nth.cpp resource_win31.cpp scaler.cpp screenshot.cpp script.cpp sfxplayer.cpp sokol_sys.cc staticres.cpp unpack.cpp util.cpp video.cpp)
    fips_deps(gfx sokol)
    if (FIPS_EMSCRIPTEN)
    else()
        fips_libs(z)
    endif()
fips_end_app()
