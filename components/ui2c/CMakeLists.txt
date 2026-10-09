add_library(ui2c STATIC)

target_sources(ui2c
    PRIVATE
        src/ui2c.c
)

target_include_directories(ui2c
    PUBLIC
        include
    PRIVATE
        src
)

add_library(ui2c_stm32f0 STATIC)

target_sources(ui2c_stm32f0
    PRIVATE
        ports/stm32f0/ui2c_stm32f0.c
)

target_include_directories(ui2c_stm32f0
    PRIVATE
        src
)

target_link_libraries(ui2c_stm32f0
    PUBLIC
        ui2c
        stm32cubemx
)
