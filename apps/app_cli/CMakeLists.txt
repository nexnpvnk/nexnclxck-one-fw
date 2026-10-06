add_library(app_cli STATIC)

target_sources(app_cli
    PRIVATE
        src/app_cli.c
        src/app_cli_clock.c
)

target_include_directories(app_cli
    PUBLIC
        include
)

target_link_libraries(app_cli
    PRIVATE
        app_clock
        wsh_shell_integration
)
