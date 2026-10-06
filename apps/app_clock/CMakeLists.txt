add_library(app_clock STATIC)

target_sources(app_clock
    PRIVATE
        src/app_clock.c
)

target_include_directories(app_clock
    PUBLIC
        include
)

target_link_libraries(app_clock
    PRIVATE
        stm32cubemx
)
