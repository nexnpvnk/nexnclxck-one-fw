#include "bsp_console.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "lwrb/lwrb.h"
#include "usart.h"

#define BSP_CONSOLE_TX_BUFFER_SIZE     64U
#define BSP_CONSOLE_TX_TIMEOUT_MS      100U
#define BSP_CONSOLE_RX_BUFFER_SIZE     64U
#define BSP_CONSOLE_RX_DMA_BUFFER_SIZE 16U

static lwrb_t tx_rb;
static uint8_t tx_buffer[BSP_CONSOLE_TX_BUFFER_SIZE];

static lwrb_t rx_rb;
static uint8_t rx_buffer[BSP_CONSOLE_RX_BUFFER_SIZE];
static uint8_t rx_dma_buffer[BSP_CONSOLE_RX_DMA_BUFFER_SIZE];

static volatile lwrb_sz_t tx_dma_len;
static volatile bool tx_dma_active;

static volatile uint16_t rx_dma_pos;
static volatile bool rx_restart_pending;
static volatile uint32_t rx_dropped;

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

static bool bsp_console_tx_recover(void) {
    lwrb_sz_t completed;
    lwrb_sz_t remaining;

    completed = 0U;

    if (huart1.hdmatx != NULL && tx_dma_len > 0U) {
        remaining = (lwrb_sz_t)__HAL_DMA_GET_COUNTER(huart1.hdmatx);

        if (remaining <= tx_dma_len) {
            completed = tx_dma_len - remaining;
        }
    }

    if (HAL_UART_AbortTransmit(&huart1) != HAL_OK) {
        return false;
    }

    /*
     * Remove bytes which DMA has already transferred to USART.
     * The remaining bytes stay in the ring buffer and are retried.
     */
    if (completed > 0U) {
        lwrb_skip(&tx_rb, completed);
    }

    tx_dma_len = 0U;
    tx_dma_active = false;

    bsp_console_tx_start();

    return true;
}

static void bsp_console_rx_write(const uint8_t* data, uint16_t len) {
    lwrb_sz_t written;

    if (len == 0U) {
        return;
    }

    if (!lwrb_write_ex(&rx_rb, data, (lwrb_sz_t)len, &written, LWRB_FLAG_WRITE_ALL)) {
        rx_dropped += len;
    }
}

static void bsp_console_rx_commit(uint16_t pos) {
    uint16_t old_pos;

    if (pos > BSP_CONSOLE_RX_DMA_BUFFER_SIZE) {
        return;
    }

    old_pos = rx_dma_pos;

    if (pos == old_pos) {
        return;
    }

    if (pos > old_pos) {
        bsp_console_rx_write(&rx_dma_buffer[old_pos], pos - old_pos);
    } else {
        bsp_console_rx_write(&rx_dma_buffer[old_pos], BSP_CONSOLE_RX_DMA_BUFFER_SIZE - old_pos);

        bsp_console_rx_write(rx_dma_buffer, pos);
    }

    rx_dma_pos = (pos == BSP_CONSOLE_RX_DMA_BUFFER_SIZE) ? 0U : pos;
}

static bool bsp_console_rx_start(void) {
    if (huart1.hdmarx == NULL) {
        return false;
    }

    rx_dma_pos = 0U;

    __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF | UART_CLEAR_PEF);

    __HAL_UART_SEND_REQ(&huart1, UART_RXDATA_FLUSH_REQUEST);

    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_dma_buffer, sizeof(rx_dma_buffer)) != HAL_OK) {
        return false;
    }

    /*
     * IDLE and transfer-complete events are enough for the console.
     * Half-transfer only creates unnecessary interrupts.
     */
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

    return true;
}

bool bsp_console_init(void) {
    if (initialized) {
        return true;
    }

    lwrb_init(&tx_rb, tx_buffer, sizeof(tx_buffer));
    lwrb_init(&rx_rb, rx_buffer, sizeof(rx_buffer));

    tx_dma_len = 0U;
    tx_dma_active = false;

    rx_dma_pos = 0U;
    rx_restart_pending = false;
    rx_dropped = 0U;

    if (!bsp_console_rx_start()) {
        return false;
    }

    initialized = true;

    return true;
}

void bsp_console_process(void) {
    if (!initialized) {
        return;
    }

    /*
     * On a TX DMA error HAL restores UART gState to READY,
     * while tx_dma_active still indicates an unfinished transfer.
     */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (tx_dma_active && huart1.gState == HAL_UART_STATE_READY) {
        (void)bsp_console_tx_recover();
    } else {
        /* Retry queued data if starting DMA previously failed. */
        bsp_console_tx_start();
    }

    __set_PRIMASK(primask);

    if (!rx_restart_pending) {
        return;
    }

    if (huart1.RxState != HAL_UART_STATE_READY) {
        return;
    }

    if (bsp_console_rx_start()) {
        rx_restart_pending = false;
    }
}

int bsp_console_read(void* data, size_t len) {
    lwrb_sz_t read;

    if (!initialized || data == NULL) {
        return -1;
    }

    if (len == 0U) {
        return 0;
    }

    read = lwrb_read(&rx_rb, data, (lwrb_sz_t)len);

    return (int)read;
}

int bsp_console_write(const void* data, size_t len) {
    if (!initialized || data == NULL || len > INT_MAX) {
        return -1;
    }

    if (len == 0U) {
        return 0;
    }

    /* DMA completion and the timeout tick must be able to interrupt us.
     * The foreground is the sole producer of the TX ring. */
    if (__get_IPSR() != 0U || __get_PRIMASK() != 0U) {
        return -1;
    }

    const uint8_t* bytes = data;
    size_t queued = 0U;
    uint32_t last_progress = HAL_GetTick();

    for (;;) {
        lwrb_sz_t written = lwrb_write(&tx_rb, bytes + queued, (lwrb_sz_t)(len - queued));

        if (written > 0U) {
            queued += written;
            last_progress = HAL_GetTick();
        }

        /* Service recovery here because the main loop is blocked in write. */
        bsp_console_process();

        /* The normal TX callback runs on UART TC, after the last stop bit. */
        if (queued == len && !tx_dma_active && lwrb_get_full(&tx_rb) == 0U) {
            return (int)len;
        }

        /* Bound a stalled transfer, not the total duration of a long message.
         * At 115200 baud the final 63 queued bytes need less than 6 ms. */
        if ((uint32_t)(HAL_GetTick() - last_progress) >= BSP_CONSOLE_TX_TIMEOUT_MS) {
            __disable_irq();

            if (HAL_UART_AbortTransmit(&huart1) == HAL_OK) {
                lwrb_reset(&tx_rb);
                tx_dma_len = 0U;
                tx_dma_active = false;
            }

            __enable_irq();
            return -1; /* An already transmitted prefix cannot be rolled back. */
        }
    }
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

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t size) {
    if (huart != &huart1) {
        return;
    }

    bsp_console_rx_commit(size);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    if (huart != &huart1) {
        return;
    }

    /*
     * In DMA RX mode UART errors are blocking in STM32 HAL.
     * HAL aborts reception before invoking this callback.
     *
     * Do not restart DMA from interrupt context. Defer it to
     * bsp_console_process().
     */
    if (huart->RxState == HAL_UART_STATE_READY) {
        rx_restart_pending = true;
    }
}
