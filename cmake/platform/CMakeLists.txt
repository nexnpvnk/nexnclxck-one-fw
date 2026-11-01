cmake_minimum_required(VERSION 3.22)
enable_language(C ASM)

# System/toolchain integration and firmware composition. Manual board drivers live in BSP.
# Current backend is CMSIS/pure registers; HAL/LL sources are not selected.
add_library(platform INTERFACE)
target_include_directories(platform INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32F0xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include
)
target_compile_definitions(platform INTERFACE
    STM32F051x8
    $<$<CONFIG:Debug>:DEBUG>
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/src/stm32f0xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/src/system_stm32f0xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32f051x8.s
)
target_link_libraries(${CMAKE_PROJECT_NAME} platform ${TOOLCHAIN_LINK_LIBRARIES})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)
