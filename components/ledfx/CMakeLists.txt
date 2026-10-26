add_library(ledfx STATIC
    src/ledfx.c
    src/ledfx_color.c
)

target_include_directories(ledfx PUBLIC
    include
)
