add_library(app_main STATIC)

target_sources(app_main
    PRIVATE
        src/app_main.c
)

target_include_directories(app_main
    PUBLIC
        include
)

target_link_libraries(app_main
    PRIVATE
        bsp
        wsh_shell_integration
)
