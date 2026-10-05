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

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

#
# Platform / vendor
#
add_subdirectory(cmake/stm32cubemx)

#
# Components
#
add_subdirectory(components/lwrb/lwrb)
add_subdirectory(components/wsh-shell-integration)

#
# Board support
#
add_subdirectory(bsp)

#
# Application
#
add_subdirectory(app)

#
# Firmware composition
#
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    app
    bsp
)

#
# Debug / programming
#
include(cmake/modules/OpenOCD.cmake)

add_openocd_targets(
    ${CMAKE_PROJECT_NAME}
    "${CMAKE_SOURCE_DIR}/debug/openocd/board.cfg"
)
