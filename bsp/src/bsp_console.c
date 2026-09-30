#include "bsp_console.h"

#include <stdbool.h>
#include <stdint.h>

#include "lwrb/lwrb.h"
#include "usart.h"

#define BSP_CONSOLE_TX_BUFFER_SIZE 512U

static lwrb_t tx_rb;
static uint8_t tx_buffer[BSP_CONSOLE_TX_BUFFER_SIZE];

static volatile lwrb_sz_t tx_dma_len;
static volatile bool tx_dma_active;
static bool initialized;

static void bsp_console_tx_start(void) {
    const uint8_t* data;
    lwrb_sz_t len;

    if (tx_dma_active) {
        return;
    }

    len = lwrb_get_linear_block_read_length(&tx_rb);

    if (len == 0U) {
        return;
    }

    data = lwrb_get_linear_block_read_address(&tx_rb);

    tx_dma_len = len;
    tx_dma_active = true;

    if (HAL_UART_Transmit_DMA(&huart1, data, (uint16_t)len) != HAL_OK) {
        tx_dma_len = 0U;
        tx_dma_active = false;
    }
}

void bsp_console_init(void) {
    lwrb_init(&tx_rb, tx_buffer, sizeof(tx_buffer));

    tx_dma_len = 0U;
    tx_dma_active = false;
    initialized = true;
}

int bsp_console_write(const void* data, size_t len) {
    lwrb_sz_t written;
    uint32_t primask;

    if (!initialized || data == NULL) {
        return -1;
    }

    if (len == 0U) {
        return 0;
    }

    if (!lwrb_write_ex(&tx_rb, data, (lwrb_sz_t)len, &written, LWRB_FLAG_WRITE_ALL)) {
        return -1;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    bsp_console_tx_start();

    if (primask == 0U) {
        __enable_irq();
    }

    return (int)written;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    if (huart != &huart1) {
        return;
    }

    lwrb_skip(&tx_rb, tx_dma_len);

    tx_dma_len = 0U;
    tx_dma_active = false;

    bsp_console_tx_start();
}
