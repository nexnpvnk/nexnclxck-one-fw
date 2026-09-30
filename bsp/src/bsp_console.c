#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "usart.h"

#define BSP_CONSOLE_TX_BUFFER_SIZE 512U

static uint8_t tx_buffer[BSP_CONSOLE_TX_BUFFER_SIZE];

static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;
static volatile uint16_t tx_dma_size;
static volatile bool tx_dma_active;

static void bsp_console_tx_start(void) {
    uint16_t size;

    if (tx_dma_active || tx_head == tx_tail) {
        return;
    }

    if (tx_head > tx_tail) {
        size = tx_head - tx_tail;
    } else {
        size = BSP_CONSOLE_TX_BUFFER_SIZE - tx_tail;
    }

    tx_dma_size = size;
    tx_dma_active = true;

    if (HAL_UART_Transmit_DMA(&huart1, &tx_buffer[tx_tail], size) != HAL_OK) {
        tx_dma_size = 0U;
        tx_dma_active = false;
    }
}

static size_t bsp_console_tx_free(void) {
    if (tx_head >= tx_tail) {
        return BSP_CONSOLE_TX_BUFFER_SIZE - (tx_head - tx_tail) - 1U;
    }

    return tx_tail - tx_head - 1U;
}

int bsp_console_write(const void* data, size_t len) {
    const uint8_t* src = data;
    uint32_t primask;
    size_t first;
    size_t free;

    if (data == NULL || len == 0U) {
        return 0;
    }

    if (len >= BSP_CONSOLE_TX_BUFFER_SIZE) {
        return -1;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    free = bsp_console_tx_free();

    if (len > free) {
        if (primask == 0U) {
            __enable_irq();
        }

        return -1;
    }

    first = BSP_CONSOLE_TX_BUFFER_SIZE - tx_head;

    if (first > len) {
        first = len;
    }

    memcpy(&tx_buffer[tx_head], src, first);

    if (len > first) {
        memcpy(tx_buffer, &src[first], len - first);
    }

    tx_head = (uint16_t)((tx_head + len) % BSP_CONSOLE_TX_BUFFER_SIZE);

    bsp_console_tx_start();

    if (primask == 0U) {
        __enable_irq();
    }

    return (int)len;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    uint32_t primask;

    if (huart != &huart1) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    tx_tail = (uint16_t)((tx_tail + tx_dma_size) % BSP_CONSOLE_TX_BUFFER_SIZE);

    tx_dma_size = 0U;
    tx_dma_active = false;

    bsp_console_tx_start();

    if (primask == 0U) {
        __enable_irq();
    }
}
