add_library(bsp STATIC)

target_sources(bsp
    PRIVATE
        src/bsp_console.c
        src/bsp_stdio.c
)

target_include_directories(bsp
    PUBLIC
        include
)

target_link_libraries(bsp
    PRIVATE
        stm32cubemx
)
