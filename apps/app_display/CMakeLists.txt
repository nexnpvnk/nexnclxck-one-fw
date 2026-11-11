add_library(app_display STATIC
    src/app_display.c
    src/display_profile.c
)
target_include_directories(app_display PUBLIC include)
target_link_libraries(app_display PRIVATE bsp app_clock PUBLIC nixie)
