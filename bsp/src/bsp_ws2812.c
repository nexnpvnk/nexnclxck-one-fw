#include "bsp_ws2812.h"

#include "stm32f0xx_hal.h"
#include "tim.h"

#define WS2812_PIN         GPIO_PIN_2
#define WS2812_BITS        24U
#define WS2812_SLOTS       3U
#define WS2812_BUFFER_SIZE (WS2812_BITS * WS2812_SLOTS)

#define WS2812_SET         ((uint32_t)WS2812_PIN)
#define WS2812_RESET       ((uint32_t)WS2812_PIN << 16U)

static uint32_t dma_buffer[WS2812_BUFFER_SIZE];

static bool initialized;
static volatile bool busy;

static void ws2812_stop(void) {
    __HAL_TIM_DISABLE_DMA(&htim2, TIM_DMA_CC3);
    (void)HAL_TIM_OC_Stop(&htim2, TIM_CHANNEL_3);

    GPIOD->BSRR = WS2812_RESET;

    busy = false;
}

static void ws2812_dma_complete(DMA_HandleTypeDef* dma) {
    (void)dma;
    ws2812_stop();
}

static void ws2812_dma_error(DMA_HandleTypeDef* dma) {
    (void)dma;
    ws2812_stop();
}

static void ws2812_encode_byte(uint8_t value, uint32_t* output) {
    for (int bit = 7; bit >= 0; --bit) {
        bool one = (value & (1U << bit)) != 0U;

        output[0] = WS2812_SET;
        output[1] = one ? 0U : WS2812_RESET;
        output[2] = one ? WS2812_RESET : 0U;

        output += WS2812_SLOTS;
    }
}

bool bsp_ws2812_init(void) {
    if (initialized) {
        return true;
    }

    DMA_HandleTypeDef* dma = htim2.hdma[TIM_DMA_ID_CC3];

    if (htim2.Instance != TIM2 || dma == NULL || dma->Instance != DMA1_Channel1
        || dma->Init.Direction != DMA_MEMORY_TO_PERIPH || dma->Init.PeriphDataAlignment != DMA_PDATAALIGN_WORD
        || dma->Init.MemDataAlignment != DMA_MDATAALIGN_WORD || dma->Init.MemInc != DMA_MINC_ENABLE
        || dma->Init.PeriphInc != DMA_PINC_DISABLE || dma->Init.Mode != DMA_NORMAL || htim2.Init.Prescaler != 0U
        || htim2.Init.Period != 19U || htim2.Instance->CCR3 != 10U) {
        return false;
    }

    dma->XferCpltCallback = ws2812_dma_complete;
    dma->XferHalfCpltCallback = NULL;
    dma->XferErrorCallback = ws2812_dma_error;

    /* DMA request on CC3 match, not timer update. */
    CLEAR_BIT(htim2.Instance->CR2, TIM_CR2_CCDS);

    GPIOD->BSRR = WS2812_RESET;

    initialized = true;
    return true;
}

bool bsp_ws2812_is_busy(void) { return busy; }

bool bsp_ws2812_show_rgb(uint8_t r, uint8_t g, uint8_t b) {
    if (!initialized || busy) {
        return false;
    }

    /* WS2812 wire order is GRB. */
    ws2812_encode_byte(g, &dma_buffer[0]);
    ws2812_encode_byte(r, &dma_buffer[24]);
    ws2812_encode_byte(b, &dma_buffer[48]);

    DMA_HandleTypeDef* dma = htim2.hdma[TIM_DMA_ID_CC3];

    busy = true;

    GPIOD->BSRR = WS2812_RESET;

    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC3);

    if (HAL_DMA_Start_IT(dma, (uint32_t)dma_buffer, (uint32_t)&GPIOD->BSRR, WS2812_BUFFER_SIZE) != HAL_OK) {
        busy = false;
        return false;
    }

    __HAL_TIM_ENABLE_DMA(&htim2, TIM_DMA_CC3);

    if (HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_3) != HAL_OK) {
        __HAL_TIM_DISABLE_DMA(&htim2, TIM_DMA_CC3);
        (void)HAL_DMA_Abort(dma);
        GPIOD->BSRR = WS2812_RESET;
        busy = false;
        return false;
    }

    return true;
}
