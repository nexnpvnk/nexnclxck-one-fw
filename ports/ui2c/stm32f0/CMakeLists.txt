
add_library(ui2c_stm32f0 STATIC)
add_library(ui2c::stm32f0 ALIAS ui2c_stm32f0)

target_sources(ui2c_stm32f0
    PRIVATE
        ui2c_stm32f0.c
)

target_include_directories(ui2c_stm32f0
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ui2c_stm32f0
    PUBLIC
        ui2c::core
)
