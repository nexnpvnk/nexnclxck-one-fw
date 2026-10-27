add_library(app_cli STATIC)

target_sources(app_cli
    PRIVATE
        src/app_cli.c
        src/app_cli_clock.c
        src/app_cli_bme280.c
        src/app_cli_buzzer.c
)

target_include_directories(app_cli
    PUBLIC
        include
)

target_link_libraries(app_cli
    PRIVATE
        app_clock
        app_bme280
        app_sound
        bsp
        wsh_shell_integration
)
