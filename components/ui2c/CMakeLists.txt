add_library(ui2c STATIC)
target_compile_features(ui2c PUBLIC c_std_11)
add_library(ui2c::core ALIAS ui2c)

target_sources(ui2c
    PRIVATE
        src/ui2c.c
)

target_include_directories(ui2c
    PUBLIC
        include
)

option(UI2C_BUILD_STM32F0 "Build the STM32F0 register backend" OFF)
if(UI2C_BUILD_STM32F0)
    add_library(ui2c_stm32f0 STATIC ports/stm32f0/ui2c_stm32f0.c)
    add_library(ui2c::stm32f0 ALIAS ui2c_stm32f0)
    target_link_libraries(ui2c_stm32f0 PUBLIC ui2c)
    # The parent supplies CMSIS device includes/defines to this target.
endif()
