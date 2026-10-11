#include "app_main.h"

#include "app_cli.h"
#include "bsp_console.h"
#include "bsp_i2c.h"
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

    if (!app_cli_init()) {
        while (1) {}
    }

    while (1) {
        bsp_i2c_process();
        shell_process();
    }
}
