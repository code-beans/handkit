# hk_add_pak(target "Display Name" [ASSETS directory])
function(hk_add_pak target display_name)
    cmake_parse_arguments(PAK "" "ASSETS" "" ${ARGN})
    if(NOT display_name MATCHES "^[A-Za-z0-9 _-]+$")
        message(FATAL_ERROR "Pak display name must contain only letters, numbers, spaces, underscores or hyphens")
    endif()
    set(folder "${CMAKE_BINARY_DIR}/dist/${display_name}.pak")
    add_custom_target(
        ${target}-pak
        COMMAND ${CMAKE_COMMAND} -E make_directory "${folder}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${target}> "${folder}/app"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/launch.sh" "${folder}/launch.sh"
        DEPENDS ${target}
        VERBATIM)
    if(PAK_ASSETS)
        add_custom_command(
            TARGET ${target}-pak
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${PAK_ASSETS}" "${folder}/assets"
            VERBATIM)
    endif()
endfunction()
