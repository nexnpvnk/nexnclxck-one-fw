#include "app_main.h"

#include "bsp_console.h"
#include "shell.h"

void app_main(void) {
    if (!bsp_console_init()) {
        while (1) {}
    }

    if (!shell_init()) {
        while (1) {}
    }

    while (1) {
        shell_process();
    }
}
