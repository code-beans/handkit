cmake_minimum_required(VERSION 3.24)
project(handkit_checks LANGUAGES C)

# Separate from the installed SDK and the device demo. Use an SDL3 build for the machine running these checks; virtual controllers need no display.
find_package(SDL3 CONFIG REQUIRED)
set(HANDKIT_DEVICE
    arkos
    CACHE STRING "Device integration to test")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../sdk" handkit)
enable_testing()

add_executable(handkit-input-check sdk_input.c)
target_link_libraries(handkit-input-check PRIVATE Handkit::Handkit)
add_test(NAME handkit-input COMMAND handkit-input-check)

if(HANDKIT_DEVICE STREQUAL "arkos")
    add_executable(handkit-arkos-check sdk_arkos.c)
    target_include_directories(handkit-arkos-check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/src")
    target_link_libraries(handkit-arkos-check PRIVATE Handkit::Handkit)
    add_test(NAME handkit-arkos COMMAND handkit-arkos-check)
endif()

find_package(
    Python3
    COMPONENTS Interpreter
    REQUIRED)
add_test(NAME arkos-gamepads COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/arkos_gamepads.py")
