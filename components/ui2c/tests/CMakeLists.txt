cmake_minimum_required(VERSION 3.22)
project(ui2c_tests C)
enable_testing()
set(UI2C_BUILD_STM32F0 OFF)
add_subdirectory(.. ui2c)
add_executable(ui2c_tests test_ui2c.c ../ports/stm32f0/ui2c_stm32f0.c)
target_link_libraries(ui2c_tests PRIVATE ui2c::core)
target_include_directories(ui2c_tests BEFORE PRIVATE
    fakes
    ../../../Drivers/CMSIS/Device/ST/STM32F0xx/Include
    ../../../Drivers/CMSIS/Include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ui2c_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME ui2c COMMAND ui2c_tests)
