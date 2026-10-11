add_library(app_bme280 STATIC src/app_bme280.c src/bme280_port.c)
target_include_directories(app_bme280 PUBLIC include)
target_link_libraries(app_bme280 PUBLIC ui2c::core PRIVATE bme280_bosch bsp)
target_compile_features(app_bme280 PUBLIC c_std_11)
