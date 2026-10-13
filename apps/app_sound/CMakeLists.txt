add_library(app_sound STATIC)

target_sources(app_sound
    PRIVATE
        src/app_sound.c
)

target_include_directories(app_sound
    PUBLIC
        include
)

target_link_libraries(app_sound
    PRIVATE
        bsp
)
