#include "bsp_console.h"
#include <limits.h>
#include "bsp_time.h"
#include "lwrb/lwrb.h"
#include "stm32f0xx.h"

#define BSP_CONSOLE_TX_BUFFER_SIZE     64U
#define BSP_CONSOLE_TX_TIMEOUT_MS      100U
#define BSP_CONSOLE_RX_BUFFER_SIZE     64U
#define BSP_CONSOLE_RX_DMA_BUFFER_SIZE 16U
#define TX_DMA_FLAGS                   (DMA_ISR_TCIF4 | DMA_ISR_HTIF4 | DMA_ISR_TEIF4)
#define RX_DMA_FLAGS                   (DMA_ISR_TCIF5 | DMA_ISR_HTIF5 | DMA_ISR_TEIF5)
#define UART_RX_ERRORS                 (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE)

static lwrb_t tx_rb;
static uint8_t tx_buffer[BSP_CONSOLE_TX_BUFFER_SIZE];
static lwrb_t rx_rb;
static uint8_t rx_buffer[BSP_CONSOLE_RX_BUFFER_SIZE];
static uint8_t rx_dma_buffer[BSP_CONSOLE_RX_DMA_BUFFER_SIZE];

static volatile lwrb_sz_t tx_dma_len;
static volatile bool tx_dma_active;
static volatile bool tx_wait_tc;
static volatile bool tx_retry_pending;
static uint16_t rx_dma_pos;
static volatile bool rx_restart_pending;
static volatile uint32_t rx_dropped;
static bool initialized;

static uint32_t lock(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return primask;
}

static void unlock(uint32_t primask) {
    __DMB();
    __set_PRIMASK(primask);
}

static void disable_dma(DMA_Channel_TypeDef* channel) {
    channel->CCR &= ~DMA_CCR_EN;
    (void)channel->CCR;
    __DSB();
}

/* Called with IRQs masked or from UART/DMA IRQs of equal preemption priority. */
static void tx_start(void) {
    if (tx_dma_active || tx_retry_pending) {
        return;
    }
    lwrb_sz_t len = lwrb_get_linear_block_read_length(&tx_rb);
    if (len == 0U || (USART1->CR1 & (USART_CR1_UE | USART_CR1_TE)) != (USART_CR1_UE | USART_CR1_TE)) {
        return;
    }
    disable_dma(DMA1_Channel4);
    DMA1->IFCR = TX_DMA_FLAGS;
    USART1->ICR = USART_ICR_TCCF;
    DMA1_Channel4->CPAR = (uint32_t)&USART1->TDR;
    DMA1_Channel4->CMAR = (uint32_t)lwrb_get_linear_block_read_address(&tx_rb);
    DMA1_Channel4->CNDTR = (uint32_t)len;
    DMA1_Channel4->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_TEIE;
    tx_dma_len = len;
    tx_wait_tc = false;
    tx_dma_active = true;
    __DMB();
    DMA1_Channel4->CCR |= DMA_CCR_EN;
    USART1->CR3 |= USART_CR3_DMAT;
}

static void tx_finish(void) {
    USART1->CR1 &= ~USART_CR1_TCIE;
    USART1->ICR = USART_ICR_TCCF;
    lwrb_skip(&tx_rb, tx_dma_len);
    tx_dma_len = 0U;
    tx_wait_tc = false;
    tx_dma_active = false;
    tx_start();
}

static void rx_write(const uint8_t* data, uint16_t len) {
    lwrb_sz_t written;
    if (len != 0U && !lwrb_write_ex(&rx_rb, data, len, &written, LWRB_FLAG_WRITE_ALL)) {
        rx_dropped += len;
    }
}

static void rx_stop(void) {
    /* Mask error/IDLE sources until the foreground restarts reception. */
    USART1->CR3 &= ~(USART_CR3_DMAR | USART_CR3_EIE);
    USART1->CR1 &= ~USART_CR1_IDLEIE;
    disable_dma(DMA1_Channel5);
    DMA1->IFCR = RX_DMA_FLAGS;
    rx_restart_pending = true;
}

static void rx_start(void) {
    disable_dma(DMA1_Channel5);
    DMA1->IFCR = RX_DMA_FLAGS;
    USART1->ICR = USART_ICR_IDLECF | USART_ICR_ORECF | USART_ICR_NCF | USART_ICR_FECF | USART_ICR_PECF;
    USART1->RQR = USART_RQR_RXFRQ;
    rx_dma_pos = 0U;
    DMA1_Channel5->CPAR = (uint32_t)&USART1->RDR;
    DMA1_Channel5->CMAR = (uint32_t)rx_dma_buffer;
    DMA1_Channel5->CNDTR = BSP_CONSOLE_RX_DMA_BUFFER_SIZE;
    DMA1_Channel5->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PL_1 | DMA_CCR_HTIE | DMA_CCR_TCIE | DMA_CCR_TEIE;
    rx_restart_pending = false;
    __DMB();
    DMA1_Channel5->CCR |= DMA_CCR_EN;
    USART1->CR3 |= USART_CR3_DMAR | USART_CR3_EIE;
    USART1->CR1 |= USART_CR1_IDLEIE;
}

/* Shared cursor/flag accounting prevents IDLE and DMA IRQs committing a wrap twice.
 * Service latency must stay below one DMA-buffer lap (16 bytes at 115200 baud). */
static void rx_service(void) {
    if (rx_restart_pending) {
        return;
    }
    uint32_t before = DMA1->ISR;
    uint32_t remaining = DMA1_Channel5->CNDTR;
    uint32_t flags = DMA1->ISR;
    if (((before ^ flags) & DMA_ISR_TCIF5) != 0U) {
        remaining = DMA1_Channel5->CNDTR;
    }
    if ((flags & DMA_ISR_TEIF5) != 0U || (USART1->ISR & UART_RX_ERRORS) != 0U) {
        rx_stop();
        USART1->ICR = USART_ICR_ORECF | USART_ICR_NCF | USART_ICR_FECF | USART_ICR_PECF;
        return;
    }
    uint16_t pos = (uint16_t)(BSP_CONSOLE_RX_DMA_BUFFER_SIZE - remaining);
    if (pos == BSP_CONSOLE_RX_DMA_BUFFER_SIZE) {
        pos = 0U;
    }
    /* CNDTR may reload before TC becomes visible. Leave the wrap for that IRQ. */
    if ((flags & DMA_ISR_TCIF5) == 0U && (remaining == 0U || pos < rx_dma_pos)) {
        return;
    }
    DMA1->IFCR = flags & (DMA_ISR_HTIF5 | DMA_ISR_TCIF5);
    __DMB();
    if ((flags & DMA_ISR_TCIF5) != 0U || pos < rx_dma_pos) {
        rx_write(&rx_dma_buffer[rx_dma_pos], BSP_CONSOLE_RX_DMA_BUFFER_SIZE - rx_dma_pos);
        rx_write(rx_dma_buffer, pos);
    } else if (pos > rx_dma_pos) {
        rx_write(&rx_dma_buffer[rx_dma_pos], pos - rx_dma_pos);
    }
    rx_dma_pos = pos;
}

bool bsp_console_init(void) {
    if (initialized) {
        return true;
    }
    if ((USART1->CR1 & (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE)) != (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE)) {
        return false;
    }
    lwrb_init(&tx_rb, tx_buffer, sizeof(tx_buffer));
    lwrb_init(&rx_rb, rx_buffer, sizeof(rx_buffer));
    uint32_t primask = lock();
    tx_dma_len = 0U;
    tx_dma_active = false;
    tx_wait_tc = false;
    tx_retry_pending = false;
    rx_dropped = 0U;
    initialized = true;
    rx_start();
    unlock(primask);
    return true;
}

void bsp_console_process(void) {
    if (!initialized) {
        return;
    }
    uint32_t primask = lock();
    if (rx_restart_pending) {
        rx_start();
    }
    if (!tx_dma_active) {
        tx_retry_pending = false;
    }
    tx_start();
    unlock(primask);
}

int bsp_console_read(void* data, size_t len) {
    if (!initialized || data == NULL) {
        return -1;
    }
    if (len == 0U) {
        return 0;
    }
    return (int)lwrb_read(&rx_rb, data, (lwrb_sz_t)len);
}

int bsp_console_write(const void* data, size_t len) {
    if (!initialized || data == NULL || len > INT_MAX) {
        return -1;
    }
    if (len == 0U) {
        return 0;
    }
    /* DMA completion and SysTick must be able to interrupt the sole TX producer. */
    if (__get_IPSR() != 0U || __get_PRIMASK() != 0U) {
        return -1;
    }

    const uint8_t* bytes = data;
    size_t queued = 0U;
    uint32_t last_progress = bsp_time_ms();

    for (;;) {
        lwrb_sz_t written = lwrb_write(&tx_rb, bytes + queued, (lwrb_sz_t)(len - queued));
        if (written > 0U) {
            queued += written;
            last_progress = bsp_time_ms();
        }

        /* The main loop cannot service recovery while blocked in write. */
        bsp_console_process();
        if (queued == len && !tx_dma_active && lwrb_get_full(&tx_rb) == 0U) {
            return (int)len; /* tx_finish runs only after USART TC. */
        }

        /* Bound a stalled transfer, not the total duration of a long message. */
        if ((uint32_t)(bsp_time_ms() - last_progress) >= BSP_CONSOLE_TX_TIMEOUT_MS) {
            uint32_t primask = lock();
            USART1->CR3 &= ~USART_CR3_DMAT;
            USART1->CR1 &= ~USART_CR1_TCIE;
            disable_dma(DMA1_Channel4);
            DMA1->IFCR = TX_DMA_FLAGS;
            USART1->ICR = USART_ICR_TCCF;
            lwrb_reset(&tx_rb);
            tx_dma_len = 0U;
            tx_dma_active = false;
            tx_wait_tc = false;
            tx_retry_pending = false;
            unlock(primask);
            return -1; /* An already transmitted prefix cannot be rolled back. */
        }
    }
}

void bsp_console_dma_irq(void) {
    if (!initialized) {
        return;
    }
    uint32_t flags = DMA1->ISR;
    if ((flags & (DMA_ISR_TCIF4 | DMA_ISR_TEIF4)) != 0U) {
        USART1->CR3 &= ~USART_CR3_DMAT;
        disable_dma(DMA1_Channel4);
        uint32_t remaining = DMA1_Channel4->CNDTR;
        DMA1->IFCR = TX_DMA_FLAGS;
        if (tx_dma_active && !tx_wait_tc) {
            if ((flags & DMA_ISR_TEIF4) != 0U) {
                /* Keep the unconsumed suffix queued; retry from foreground. */
                tx_dma_len = remaining <= tx_dma_len ? tx_dma_len - remaining : 0U;
                tx_retry_pending = true;
            }
            /* DMA TC only means TDR was filled; wait for the last stop bit. */
            tx_wait_tc = true;
            if (tx_dma_len == 0U || (USART1->ISR & USART_ISR_TC) != 0U) {
                tx_finish();
            } else {
                USART1->CR1 |= USART_CR1_TCIE;
            }
        }
    }
    if ((flags & RX_DMA_FLAGS) != 0U) {
        rx_service();
    }
}

void bsp_console_uart_irq(void) {
    if (!initialized) {
        return;
    }
    if ((USART1->ISR & (USART_ISR_IDLE | UART_RX_ERRORS)) != 0U) {
        USART1->ICR = USART_ICR_IDLECF;
        rx_service();
    }
    if (tx_wait_tc && (USART1->ISR & USART_ISR_TC) != 0U) {
        tx_finish();
    }
}
