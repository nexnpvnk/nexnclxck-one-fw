add_library(app_input STATIC
    src/app_input.c
)

target_include_directories(app_input PUBLIC
    include
)

target_link_libraries(app_input
    PRIVATE
        bsp
        lwbtn
        microlog
)
