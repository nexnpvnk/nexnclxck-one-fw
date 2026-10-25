
# Standalone configuration
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.22)
    project(ui2c VERSION 1.0.0 LANGUAGES C)
endif()

add_library(ui2c STATIC)
add_library(ui2c::core ALIAS ui2c)

target_sources(ui2c
    PRIVATE
        src/ui2c.c
)

target_include_directories(ui2c
    PUBLIC
        include
)

target_compile_features(ui2c PUBLIC c_std_11)
