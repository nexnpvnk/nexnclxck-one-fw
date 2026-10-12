#
# BSP is an OBJECT library to ensure linker hooks are included
# directly in the final firmware image.
#
add_library(bsp OBJECT)

target_sources(bsp
    PRIVATE
        src/bsp_console.c
        src/bsp_stdio.c
        src/bsp_i2c.c
        src/bsp_time.c
        src/bsp_flash_fal.c
        src/bsp_storage.c
)

target_include_directories(bsp
    PUBLIC
        include
)

target_link_libraries(bsp
    PRIVATE
        stm32cubemx
        lwrb
        ui2c_stm32f0
)

# Public board API exposes only the portable bus handle.
target_link_libraries(bsp PUBLIC ui2c flashdb)
