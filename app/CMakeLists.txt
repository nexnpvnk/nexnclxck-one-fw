add_library(app STATIC)

target_sources(app
    PRIVATE
        src/app_main.c
)

target_include_directories(app
    PUBLIC
        include
)

target_link_libraries(app
    PRIVATE
        bsp
)
