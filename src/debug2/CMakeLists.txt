add_library(libdebug2 STATIC
	debug2.cpp
)

target_link_libraries(libdebug2 PRIVATE libfmt imgui $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>)
