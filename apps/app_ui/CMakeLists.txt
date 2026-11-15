add_library(app_ui STATIC src/app_ui.c)
target_include_directories(app_ui PUBLIC include)
target_link_libraries(app_ui PUBLIC nixie PRIVATE app_display app_clock app_bme280 app_light app_sound bsp)
