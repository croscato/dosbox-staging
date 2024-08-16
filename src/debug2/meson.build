libdebug2_sources = files(
    'debug2.cpp',
)

libdebug2 = static_library(
    'debug2',
    libdebug2_sources,
    include_directories: incdir,
    dependencies: [
        sdl2_dep,
        libpdcurses_dep,
        libfmt_dep,
        imgui_dep,
    ],
    cpp_args: warnings,
)

libdebug2_dep = declare_dependency(link_with: libdebug2)

internal_deps += libdebug2_dep
