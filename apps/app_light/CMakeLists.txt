add_library(app_light STATIC
    src/app_light.c
)

target_include_directories(app_light PUBLIC
    include
)

target_link_libraries(app_light
    PUBLIC
        ledfx
    PRIVATE
        bsp
)
