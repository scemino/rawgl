fips_begin_app(raw windowed)
    fips_files(raw.c game.h)
    fips_deps(sokol miniz)
fips_end_app()

fips_begin_app(raw-ui windowed)
    fips_files(raw.c game.h raw-ui-impl.cc)
    fips_deps(sokol miniz)
    fips_deps(ui)
fips_end_app()
target_compile_definitions(raw-ui PRIVATE GAME_USE_UI)