set(OPENOCD_EXECUTABLE
    "openocd"
    CACHE STRING
    "OpenOCD executable"
)

function(add_openocd_targets TARGET OPENOCD_CONFIG)

    add_custom_target(flash
        COMMAND
            "${OPENOCD_EXECUTABLE}"
            -f "${OPENOCD_CONFIG}"
            -c "program {$<TARGET_FILE:${TARGET}>} verify reset exit"

        DEPENDS
            ${TARGET}

        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(erase
        COMMAND
            "${OPENOCD_EXECUTABLE}"
            -f "${OPENOCD_CONFIG}"
            -c "init; reset halt; flash erase_sector 0 0 last; flash erase_check 0; shutdown"

        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(reset
        COMMAND
            "${OPENOCD_EXECUTABLE}"
            -f "${OPENOCD_CONFIG}"
            -c "init; reset run; shutdown"

        USES_TERMINAL
        VERBATIM
    )

endfunction()
