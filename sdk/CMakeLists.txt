cmake_minimum_required(VERSION 3.24)
project(
    handkit
    VERSION 0.4.0
    LANGUAGES C)
find_package(SDL3 CONFIG REQUIRED)
add_library(handkit STATIC src/runtime.c src/feedback.c src/menu.c src/system.c)
add_library(Handkit::Handkit ALIAS handkit)
target_link_libraries(handkit PUBLIC SDL3::SDL3 ${CMAKE_DL_LIBS})
target_include_directories(handkit PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
install(TARGETS handkit ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY cmake/ DESTINATION lib/cmake/Handkit)
