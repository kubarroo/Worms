set(SDL_SHARED ON CACHE BOOL "")
set(SDL_STATIC OFF CACHE BOOL "")
set(SDL_TEST OFF CACHE BOOL "")

# PNG loading through the bundled stb backend
set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG_SAVE OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JPG_SAVE OFF CACHE BOOL "" FORCE)

# Unused image codecs with external dependencies
set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)

# Required audio formats
set(SDL2MIXER_WAVE ON CACHE BOOL "" FORCE)
set(SDL2MIXER_VORBIS "STB" CACHE STRING "" FORCE)

# Unused audio codecs
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3 OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME OFF CACHE BOOL "" FORCE)

set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "")
set(SDL2IMAGE_TESTS OFF CACHE BOOL "")
set(SDL2IMAGE_VENDORED ON CACHE BOOL "")
set(SDL_LIBC ON CACHE BOOL "Use the system C runtime" FORCE)

set(SDL2MIXER_SAMPLES OFF CACHE BOOL "")
set(SDL2MIXER_VENDORED ON CACHE BOOL "")

set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "")
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "")
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "")

set(BUILD_SHARED_LIBS ON CACHE BOOL "")

add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
add_subdirectory(third_party/SDL_image EXCLUDE_FROM_ALL)
add_subdirectory(third_party/SDL_mixer EXCLUDE_FROM_ALL)
add_subdirectory(third_party/box2d EXCLUDE_FROM_ALL)

set(IMGUI_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/imgui")

add_library(worms_imgui STATIC
    "${IMGUI_SOURCE_DIR}/imgui.cpp"
    "${IMGUI_SOURCE_DIR}/imgui_draw.cpp"
    "${IMGUI_SOURCE_DIR}/imgui_tables.cpp"
    "${IMGUI_SOURCE_DIR}/imgui_widgets.cpp"
    "${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
    "${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp"
)

target_include_directories(worms_imgui PUBLIC
    "${IMGUI_SOURCE_DIR}"
    "${IMGUI_SOURCE_DIR}/backends"
)

target_link_libraries(worms_imgui PUBLIC SDL2::SDL2)