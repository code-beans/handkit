cmake_minimum_required(VERSION 3.24)
project(handkit_examples LANGUAGES C)
option(HANDKIT "Build the handheld demos with the Conan SDK" ON)
if(NOT HANDKIT)
    find_package(SDL3 3.2 CONFIG REQUIRED)
    add_executable(handkit-sdl3 examples/demo/main.c)
    target_include_directories(handkit-sdl3 PRIVATE sdk/include)
    target_link_libraries(handkit-sdl3 PRIVATE SDL3::SDL3)
    return()
endif()
find_package(Handkit CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
foreach(mode software gles vulkan)
    if(mode STREQUAL "vulkan")
        add_library(demo-${mode} OBJECT examples/demo/demo3d.c)
    else()
        add_library(demo-${mode} OBJECT examples/demo/main.c)
    endif()
    target_link_libraries(demo-${mode} PRIVATE Handkit::Handkit)
    target_compile_definitions(demo-${mode} PRIVATE DEMO_COMBINED=1 HK_SDK=1 DEMO_ENTRY=hk_${mode}_demo)
endforeach()
target_compile_definitions(demo-software PRIVATE HK_FB=1)
target_compile_definitions(demo-gles PRIVATE HK_GPU=1)
add_executable(handkit-demo examples/demo/demo_launcher.c $<TARGET_OBJECTS:demo-software> $<TARGET_OBJECTS:demo-gles> $<TARGET_OBJECTS:demo-vulkan>)
target_link_libraries(handkit-demo PRIVATE Handkit::Handkit)
find_program(GLSLANG_VALIDATOR NAMES glslangValidator glslang REQUIRED)
foreach(shader cube.vert cube.frag hud.vert hud.frag)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/${shader}.spv"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
        COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.0 "${CMAKE_CURRENT_SOURCE_DIR}/examples/demo/shaders/${shader}" -o "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/${shader}.spv"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/demo/shaders/${shader}"
        VERBATIM)
    list(APPEND DEMO_SHADERS "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/${shader}.spv")
endforeach()
add_custom_target(demo-shaders ALL DEPENDS ${DEMO_SHADERS})
add_dependencies(handkit-demo demo-shaders)
hk_add_pak(handkit-demo "SDL3 Demo" ASSETS "${CMAKE_CURRENT_BINARY_DIR}/assets")
add_executable(handkit-minimal examples/minimal/main.c)
target_link_libraries(handkit-minimal PRIVATE Handkit::Handkit)
hk_add_pak(handkit-minimal "SDK Minimal")

add_executable(sdk-input-check tests/sdk_input.c)
target_link_libraries(sdk-input-check PRIVATE Handkit::Handkit)

# Device test package for testers: the demo, the input self-test and a script that writes a report.
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE HANDKIT_GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT HANDKIT_GIT_VERSION)
    set(HANDKIT_GIT_VERSION "unknown")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/handkit-version.txt" "${HANDKIT_GIT_VERSION}\n")
set(HANDKIT_TEST_PAK "${CMAKE_BINARY_DIR}/dist/Handkit Test.pak")
add_custom_target(
    handkit-test-pak
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/dist/SDL3 Demo.pak" "${HANDKIT_TEST_PAK}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sdk-input-check> "${HANDKIT_TEST_PAK}/sdk-input-check"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/tests/device/launch.sh" "${CMAKE_CURRENT_SOURCE_DIR}/tests/device/run-tests.sh" "${CMAKE_CURRENT_SOURCE_DIR}/tests/device/README.txt" "${HANDKIT_TEST_PAK}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/handkit-version.txt" "${HANDKIT_TEST_PAK}/VERSION"
    DEPENDS handkit-demo-pak sdk-input-check
    VERBATIM)
