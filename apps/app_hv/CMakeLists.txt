add_library(app_hv STATIC src/app_hv.c)

target_include_directories(app_hv PUBLIC include)

target_link_libraries(app_hv PRIVATE bsp)
