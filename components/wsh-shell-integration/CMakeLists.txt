set(WSH_SHELL_DIR
    ${CMAKE_CURRENT_LIST_DIR}/../wsh-shell
)

add_library(wsh_shell STATIC)

target_sources(wsh_shell
    PRIVATE
        ${WSH_SHELL_DIR}/src/wsh_shell.c
        ${WSH_SHELL_DIR}/src/wsh_shell_autocomplete.c
        ${WSH_SHELL_DIR}/src/wsh_shell_cmd.c
        ${WSH_SHELL_DIR}/src/wsh_shell_cmd_def.c
        ${WSH_SHELL_DIR}/src/wsh_shell_esc.c
        ${WSH_SHELL_DIR}/src/wsh_shell_history.c
        ${WSH_SHELL_DIR}/src/wsh_shell_interact.c
        ${WSH_SHELL_DIR}/src/wsh_shell_io.c
        ${WSH_SHELL_DIR}/src/wsh_shell_misc.c
        ${WSH_SHELL_DIR}/src/wsh_shell_promptwait.c
        ${WSH_SHELL_DIR}/src/wsh_shell_ps1_custom.c
        ${WSH_SHELL_DIR}/src/wsh_shell_session.c
        ${WSH_SHELL_DIR}/src/wsh_shell_str.c
        ${WSH_SHELL_DIR}/src/wsh_shell_user.c
)

target_include_directories(wsh_shell
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${WSH_SHELL_DIR}/src
)

target_compile_definitions(wsh_shell
    PUBLIC
        $<$<CONFIG:Debug>:WSH_SHELL_ASSERT_ENABLE>
        $<$<CONFIG:Debug>:WSH_SHELL_DEBUG_ENABLE>
)

add_library(wsh_shell_integration STATIC)

target_sources(wsh_shell_integration
    PRIVATE
        src/shell.c
)

target_include_directories(wsh_shell_integration
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(wsh_shell_integration
    PRIVATE
        bsp
        wsh_shell
)
