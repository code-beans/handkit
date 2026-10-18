cmake_minimum_required(VERSION 3.24)
project(handkit_sdl_package C)
set(SDL_INSTALL
    ON
    CACHE BOOL "" FORCE)
set(CMAKE_INSTALL_LIBDIR
    lib
    CACHE STRING "" FORCE)
set(SDL_SHARED
    OFF
    CACHE BOOL "" FORCE)
set(SDL_STATIC
    ON
    CACHE BOOL "" FORCE)
set(SDL_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY
    OFF
    CACHE BOOL "" FORCE)
set(SDL_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
set(SDL_X11
    OFF
    CACHE BOOL "" FORCE)
set(SDL_WAYLAND
    OFF
    CACHE BOOL "" FORCE)
set(SDL_KMSDRM
    OFF
    CACHE BOOL "" FORCE)
set(SDL_OPENGL
    OFF
    CACHE BOOL "" FORCE)
set(SDL_OPENGLES
    ON
    CACHE BOOL "" FORCE)
set(SDL_VULKAN
    ON
    CACHE BOOL "" FORCE)
set(SDL_AUDIO
    ON
    CACHE BOOL "" FORCE)
set(SDL_OSS
    ON
    CACHE BOOL "" FORCE)
set(SDL_ALSA
    OFF
    CACHE BOOL "" FORCE)
set(SDL_HIDAPI
    ON
    CACHE BOOL "" FORCE)
set(SDL_HIDAPI_LIBUSB
    OFF
    CACHE BOOL "" FORCE)
set(SDL_LIBUDEV
    OFF
    CACHE BOOL "" FORCE)
set(SDL_UNIX_CONSOLE_BUILD
    ON
    CACHE BOOL "" FORCE)
add_subdirectory(sdl)
target_sources(SDL3-static PRIVATE backend/sdl_handkit_video.c backend/sdl_handkit_vulkan.c)
target_compile_definitions(SDL3-static PRIVATE SDL_VIDEO_DRIVER_PRIVATE=1)
