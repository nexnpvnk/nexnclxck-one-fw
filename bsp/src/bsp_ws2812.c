
#include "bsp_ws2812.h"

#include "bsp_time.h"
#include "tim.h"

#include <string.h>

#define WS2812_PIN           (1U << 2U)
#define WS2812_WORDS_PER_LED 72U
#define WS2812_LEDS_PER_HALF 1U
#define WS2812_HALF_WORDS    (WS2812_WORDS_PER_LED * WS2812_LEDS_PER_HALF)
#define WS2812_BUFFER_WORDS  (WS2812_HALF_WORDS * 2U)

#define WS2812_SET           ((uint32_t)WS2812_PIN)
#define WS2812_RESET         ((uint32_t)WS2812_PIN << 16U)

/* Two millisecond ticks guarantee more than 1 ms LOW
 * even if the completion timestamp is taken just before SysTick.
 * This exceeds the reset time of conventional WS2812 variants. */
#define WS2812_LATCH_MS      2U

/* Maximum expected transfer is about 360 us for 12 LEDs.
 * Allow a generous margin for interrupt latency. */
#define WS2812_TIMEOUT_MS    5U

static uint32_t dma_buffer[WS2812_BUFFER_WORDS];
static bsp_ws2812_rgb_t frame[BSP_WS2812_MAX_LEDS];

static bool initialized;
static volatile bool busy;
static volatile bool latch_pending;

static volatile uint32_t finished_ms;
static uint32_t started_ms;

static volatile uint32_t completed_count;
static volatile uint32_t failed_count;
static volatile uint32_t missed_deadline_count;
static volatile uint32_t timeout_count;
static volatile bsp_ws2812_error_t last_error;

static uint8_t frame_count;
static uint8_t next_led;
static uint8_t completed_halves;
static uint8_t required_halves;

static uint32_t* encode_byte(uint32_t* output, uint8_t value) {
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        bool one = (value & (0x80U >> bit)) != 0U;

        *output++ = WS2812_SET;
        *output++ = one ? 0U : WS2812_RESET;
        *output++ = one ? WS2812_RESET : 0U;
    }

    return output;
}

static void fill_half(uint32_t* output) {
    for (uint8_t i = 0U; i < WS2812_LEDS_PER_HALF; ++i) {
        if (next_led < frame_count) {
            const bsp_ws2812_rgb_t* color = &frame[next_led++];

            /* WS2812 transmits GRB, most significant bit first. */
            output = encode_byte(output, color->g);
            output = encode_byte(output, color->r);
            output = encode_byte(output, color->b);
        } else {
            /* Inactive DMA data must never raise PD2.
             * This also protects the circular wrap at frame end. */
            for (uint32_t j = 0U; j < WS2812_WORDS_PER_LED; ++j) {
                *output++ = WS2812_RESET;
            }
        }
    }
}

static void stop_transfer(bsp_ws2812_error_t error) {
    TIM2->DIER &= ~TIM_DIER_CC3DE;
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM2->CCER &= ~TIM_CCER_CC3E;
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    (void)DMA1_Channel1->CCR;
    __DSB();
    /* ES0202: disable the channel first and clear individual flags, not GIF. */
    DMA1->IFCR = DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1;

    /* Force LOW regardless of transfer outcome. */
    GPIOD->BSRR = WS2812_RESET;

    if (error == BSP_WS2812_ERROR_NONE) {
        ++completed_count;
    } else {
        last_error = error;
        ++failed_count;

        if (error == BSP_WS2812_ERROR_DEADLINE) {
            ++missed_deadline_count;
        } else if (error == BSP_WS2812_ERROR_TIMEOUT) {
            ++timeout_count;
        }
    }

    finished_ms = bsp_time_ms();
    latch_pending = true;
    busy = false;
}

static bool dma_phase_valid(bool first_half) {
    uint32_t remaining = DMA1_Channel1->CNDTR;

    if (first_half) {
        /* After HT, DMA must still be reading half 1. */
        return remaining > 0U && remaining <= WS2812_HALF_WORDS;
    }

    /* After TC, DMA must still be reading half 0. */
    return remaining > WS2812_HALF_WORDS;
}

static void half_completed(uint32_t* completed_half, bool first_half) {
    if (!dma_phase_valid(first_half)) {
        stop_transfer(BSP_WS2812_ERROR_DEADLINE);
        return;
    }

    ++completed_halves;

    if (completed_halves >= required_halves) {
        stop_transfer(BSP_WS2812_ERROR_NONE);
        return;
    }

    /* Refill the half not currently being consumed. */
    fill_half(completed_half);

    /* Check that DMA did not catch up during refill. */
    if (!dma_phase_valid(first_half)) {
        stop_transfer(BSP_WS2812_ERROR_DEADLINE);
    }
}

void bsp_ws2812_dma_irq(void) {
    uint32_t flags = DMA1->ISR;
    if (!busy) {
        DMA1->IFCR = DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1;
        return;
    }
    if ((flags & DMA_ISR_TEIF1) != 0U) {
        stop_transfer(BSP_WS2812_ERROR_DMA);
    } else if ((flags & (DMA_ISR_HTIF1 | DMA_ISR_TCIF1)) == (DMA_ISR_HTIF1 | DMA_ISR_TCIF1)) {
        stop_transfer(BSP_WS2812_ERROR_DEADLINE);
    } else if ((flags & DMA_ISR_HTIF1) != 0U) {
        DMA1->IFCR = DMA_IFCR_CHTIF1;
        half_completed(&dma_buffer[0], true);
    } else if ((flags & DMA_ISR_TCIF1) != 0U) {
        DMA1->IFCR = DMA_IFCR_CTCIF1;
        half_completed(&dma_buffer[WS2812_HALF_WORDS], false);
    }
}

bool bsp_ws2812_init(void) {
    if (initialized) {
        return true;
    }

    const uint32_t dma_mask =
        DMA_CCR_EN | DMA_CCR_DIR | DMA_CCR_PINC | DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PSIZE | DMA_CCR_MSIZE;
    const uint32_t dma_expected = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1;
    if (SystemCoreClock != 48000000U || (RCC->CFGR & RCC_CFGR_PPRE) != 0U || (RCC->APB1ENR & RCC_APB1ENR_TIM2EN) == 0U
        || TIM2->PSC != 0U || TIM2->ARR != 19U || TIM2->CCR3 != 10U
        || (DMA1_Channel1->CCR & dma_mask) != dma_expected) {
        return false;
    }

    /* CC3 compare match generates DMA requests. */
    TIM2->CR2 &= ~TIM_CR2_CCDS;

    GPIOD->BSRR = WS2812_RESET;

    busy = false;
    latch_pending = false;
    initialized = true;

    return true;
}

bool bsp_ws2812_is_busy(void) {
    uint32_t now = bsp_time_ms();

    if (busy && (uint32_t)(now - started_ms) >= WS2812_TIMEOUT_MS) {

        /*
         * Called from foreground only.
         * Exclude the DMA ISR while aborting a stuck transfer.
         */
        uint32_t irq_enabled = NVIC_GetEnableIRQ(DMA1_Channel1_IRQn);

        NVIC_DisableIRQ(DMA1_Channel1_IRQn);

        if (busy && (uint32_t)(bsp_time_ms() - started_ms) >= WS2812_TIMEOUT_MS) {
            stop_transfer(BSP_WS2812_ERROR_TIMEOUT);
        }

        if (irq_enabled != 0U) {
            NVIC_EnableIRQ(DMA1_Channel1_IRQn);
        }
    }

    if (busy) {
        return true;
    }

    if (latch_pending) {
        if ((uint32_t)(bsp_time_ms() - finished_ms) < WS2812_LATCH_MS) {
            return true;
        }

        latch_pending = false;
    }

    return false;
}

bool bsp_ws2812_show(const bsp_ws2812_rgb_t* colors, size_t count) {
    if (!initialized || colors == NULL || count == 0U || count > BSP_WS2812_MAX_LEDS || bsp_ws2812_is_busy()) {
        return false;
    }

    if ((DMA1_Channel1->CCR & DMA_CCR_EN) != 0U || (TIM2->CR1 & TIM_CR1_CEN) != 0U) {
        stop_transfer(BSP_WS2812_ERROR_START);
        return false;
    }

    memcpy(frame, colors, count * sizeof(frame[0]));

    frame_count = (uint8_t)count;
    next_led = 0U;
    completed_halves = 0U;
    required_halves = (uint8_t)((count + WS2812_LEDS_PER_HALF - 1U) / WS2812_LEDS_PER_HALF);

    /* Prime both DMA halves before starting the timer. */
    fill_half(&dma_buffer[0]);
    fill_half(&dma_buffer[WS2812_HALF_WORDS]);

    busy = true;
    started_ms = bsp_time_ms();

    GPIOD->BSRR = WS2812_RESET;

    TIM2->CNT = 0U;
    TIM2->SR = ~TIM_SR_CC3IF;
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    (void)DMA1_Channel1->CCR;
    __DSB();
    DMA1->IFCR = DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1;
    DMA1_Channel1->CPAR = (uint32_t)&GPIOD->BSRR;
    DMA1_Channel1->CMAR = (uint32_t)dma_buffer;
    DMA1_Channel1->CNDTR = WS2812_BUFFER_WORDS;
    DMA1_Channel1->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PL | DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1
                         | DMA_CCR_HTIE | DMA_CCR_TCIE | DMA_CCR_TEIE;
    __DMB();
    DMA1_Channel1->CCR |= DMA_CCR_EN;
    TIM2->DIER |= TIM_DIER_CC3DE;
    TIM2->CCER |= TIM_CCER_CC3E;
    TIM2->CR1 |= TIM_CR1_CEN;

    return true;
}

uint32_t bsp_ws2812_error_count(void) { return failed_count; }

bool bsp_ws2812_get_stats(bsp_ws2812_stats_t* stats) {
    if (stats == NULL || !initialized) {
        return false;
    }

    /*
     * Take a consistent snapshot of values
     * also modified from the DMA interrupt.
     */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    *stats = (bsp_ws2812_stats_t){
        .completed = completed_count,
        .failed = failed_count,
        .missed_deadlines = missed_deadline_count,
        .timeouts = timeout_count,
        .last_error = last_error,
    };

    if ((primask & 1U) == 0U) {
        __enable_irq();
    }

    return true;
}
