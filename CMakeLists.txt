cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME nexnclxck-one-fw)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)

    check_ipo_supported(
        RESULT IPO_SUPPORTED
        OUTPUT IPO_ERROR
        LANGUAGES C
    )

    if(IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    else()
        message(WARNING "IPO/LTO is not supported: ${IPO_ERROR}")
    endif()
endif()

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

target_link_options(${CMAKE_PROJECT_NAME}
    PRIVATE
        -Wl,-u,_fstat
        -Wl,-u,_isatty
)

#
# Platform / vendor
#
add_subdirectory(cmake/stm32cubemx)

#
# Components
#
add_subdirectory(components/lwrb/lwrb)
set(UI2C_BUILD_STM32F0 ON)
add_subdirectory(components/ui2c)
# Unmodified Bosch SensorAPI; the board/application adapter lives in app_bme280.
add_library(bme280_bosch STATIC components/bme280/bme280.c)
target_include_directories(bme280_bosch PUBLIC components/bme280)
target_compile_definitions(bme280_bosch PUBLIC BME280_64BIT_ENABLE)
target_compile_options(bme280_bosch PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-ffunction-sections>)
# The register backend needs only the device headers, not the vendor HAL.
target_include_directories(ui2c_stm32f0 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F0xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_compile_definitions(ui2c_stm32f0 PUBLIC STM32F051x8)

#
# Board support
#
add_subdirectory(bsp)

#
# Integrations
#
add_subdirectory(components/wsh-shell-integration)

#
# Applications
#
add_subdirectory(apps)

#
# Firmware composition
#
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    bsp
    app_main
)

#
# Debug / programming
#
include(cmake/modules/OpenOCD.cmake)

add_openocd_targets(
    ${CMAKE_PROJECT_NAME}
    "${CMAKE_SOURCE_DIR}/debug/openocd/board.cfg"
)
