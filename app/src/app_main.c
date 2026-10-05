#include <stdint.h>
#include <stdio.h>

#include "app_main.h"

#include "bsp_console.h"

void app_main(void) {
    uint8_t data[32];

    if (!bsp_console_init()) {
        while (1) {}
    }

    printf("Console ready\r\n");

    while (1) {
        int len;

        bsp_console_process();

        len = bsp_console_read(data, sizeof(data));

        if (len > 0) {
            bsp_console_write(data, (size_t)len);
        }
    }
}
