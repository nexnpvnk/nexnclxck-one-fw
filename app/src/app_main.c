#include <stdint.h>
#include <stdio.h>

#include "app_main.h"

#include "bsp_console.h"

void app_main(void) {
    bsp_console_init();

    while (1) {
        printf("Alive\r\n");
    }
}
