#include <stdint.h>

#include "bsp_console.h"

#include "usart.h"

int bsp_console_write(const void* data, size_t len) {
    if (HAL_UART_Transmit(&huart1, (uint8_t*)data, (uint16_t)len, HAL_MAX_DELAY) != HAL_OK) {
        return -1;
    }

    return (int)len;
}
