#include "app_main.h"

#include "app_bme280.h"
#include "app_cli.h"
#include "app_sound.h"
#include "bsp_buzzer.h"
#include "bsp_console.h"
#include "bsp_i2c.h"
#include "bsp_storage.h"
#include "shell.h"

void app_main(void) {
    if (!bsp_i2c_init()) {
        while (1) {}
    }
    if (!bsp_console_init()) {
        while (1) {}
    }

    if (!shell_init()) {
        while (1) {}
    }

    /* Keep the console available for diagnostics if storage initialization fails. */
    (void)bsp_storage_init();

    /* Sound is optional; a failed buzzer must not prevent the clock from running. */
    (void)bsp_buzzer_init();

    if (!app_cli_init()) {
        while (1) {}
    }

    (void)app_sound_beep(2800U, 10U, 100U);
    while (1) {
        app_sound_process();
        bsp_i2c_process();
        app_bme280_process();
        shell_process();
        app_cli_process();
    }
}
