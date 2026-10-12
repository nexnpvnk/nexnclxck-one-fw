#include "bsp_buzzer.h"

#include "tim.h"

#define BSP_BUZZER_TIMER_MAX_PERIOD 65536U

static bool initialized;
static bool enabled;
static uint32_t frequency_hz = BSP_BUZZER_FREQ_DEFAULT_HZ;

static uint32_t timer_tick_hz(void) {
    uint32_t clock_hz = HAL_RCC_GetPCLK1Freq();

    /* STM32F0 timer clock doubles when
     * the APB prescaler is not 1. */
    if ((RCC->CFGR & RCC_CFGR_PPRE_2) != 0U) {
        clock_hz *= 2U;
    }

    return clock_hz / (htim16.Init.Prescaler + 1U);
}

static bool configure_frequency(uint32_t requested_hz) {
    if (requested_hz < BSP_BUZZER_FREQ_MIN_HZ || requested_hz > BSP_BUZZER_FREQ_MAX_HZ) {
        return false;
    }

    uint32_t tick_hz = timer_tick_hz();

    if (tick_hz == 0U) {
        return false;
    }

    /* Round to the nearest realizable
     * whole timer period. */
    uint32_t period = (tick_hz + requested_hz / 2U) / requested_hz;

    if (period < 2U || period > BSP_BUZZER_TIMER_MAX_PERIOD) {
        return false;
    }

    /* ARR and CCR1 are preloaded by CubeMX.
     * Prevent an update event between the writes
     * so the next PWM cycle uses a matching pair. */
    TIM_TypeDef* timer = htim16.Instance;
    uint32_t cr1 = timer->CR1;

    timer->CR1 = cr1 | TIM_CR1_UDIS;

    __HAL_TIM_SET_AUTORELOAD(&htim16, period - 1U);

    __HAL_TIM_SET_COMPARE(&htim16, TIM_CHANNEL_1, enabled ? period / 2U : 0U);

    timer->CR1 = cr1;

    return true;
}

bool bsp_buzzer_init(void) {
    if (initialized) {
        return true;
    }

    if (htim16.Instance != TIM16 || !configure_frequency(BSP_BUZZER_FREQ_DEFAULT_HZ)) {
        return false;
    }

    /* Load the initially silent PWM registers
     * before connecting TIM16_CH1. */
    __HAL_TIM_SET_COUNTER(&htim16, 0U);

    htim16.Instance->EGR = TIM_EGR_UG;

    __HAL_TIM_CLEAR_FLAG(&htim16, TIM_FLAG_UPDATE);

    if (HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1) != HAL_OK) {
        return false;
    }

    frequency_hz = BSP_BUZZER_FREQ_DEFAULT_HZ;
    enabled = false;
    initialized = true;

    return true;
}

bool bsp_buzzer_set_frequency(uint32_t requested_hz) {
    if (!initialized || !configure_frequency(requested_hz)) {
        return false;
    }

    frequency_hz = requested_hz;

    return true;
}

bool bsp_buzzer_set_enabled(bool on) {
    if (!initialized) {
        return false;
    }

    uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim16) + 1U;

    __HAL_TIM_SET_COMPARE(&htim16, TIM_CHANNEL_1, on ? period / 2U : 0U);

    enabled = on;

    return true;
}

bool bsp_buzzer_get_status(bsp_buzzer_status_t* status) {
    if (!initialized || status == NULL) {
        return false;
    }

    status->frequency_hz = frequency_hz;
    status->enabled = enabled;

    return true;
}
