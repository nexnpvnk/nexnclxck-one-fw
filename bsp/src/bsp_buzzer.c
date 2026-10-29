#include "bsp_buzzer.h"

#include <stddef.h>
#include "tim.h"

#define BSP_BUZZER_TIMER_MAX_PERIOD 65536U

static bool initialized;
static bool enabled;
static uint32_t frequency_hz = BSP_BUZZER_FREQ_DEFAULT_HZ;
static uint8_t volume_percent = BSP_BUZZER_VOLUME_DEFAULT_PERCENT;

static uint32_t timer_tick_hz(void) {
    uint32_t clock_hz = SystemCoreClock;
    uint32_t divider = (RCC->CFGR & RCC_CFGR_PPRE) >> RCC_CFGR_PPRE_Pos;

    /* STM32F0 timer clock doubles when APB prescaler != 1. */
    if (divider >= 4U) {
        clock_hz >>= divider - 3U;
        clock_hz *= 2U;
    }

    return clock_hz / (TIM16->PSC + 1U);
}

static uint32_t calculate_compare(uint32_t period) {
    if (!enabled || volume_percent == 0U) {
        return 0U;
    }

    /* Volume 0..100% corresponds to PWM duty 0..50%.
     * This is a drive level, not calibrated acoustic volume. */
    uint32_t max_compare = period / 2U;

    return (max_compare * volume_percent + 50U) / 100U;
}

static bool configure_frequency(uint32_t requested_hz) {
    if (requested_hz < BSP_BUZZER_FREQ_MIN_HZ || requested_hz > BSP_BUZZER_FREQ_MAX_HZ) {
        return false;
    }

    uint32_t tick_hz = timer_tick_hz();

    if (tick_hz == 0U) {
        return false;
    }

    /* Round to the nearest realizable whole timer period. */
    uint32_t period = (tick_hz + requested_hz / 2U) / requested_hz;

    if (period < 2U || period > BSP_BUZZER_TIMER_MAX_PERIOD) {
        return false;
    }

    /* ARR and CCR1 are preloaded by platform_tim16_init.
     * Prevent an update event between the writes
     * so the next PWM cycle uses a matching pair. */
    TIM_TypeDef* timer = TIM16;
    uint32_t cr1 = timer->CR1;

    timer->CR1 = cr1 | TIM_CR1_UDIS;

    TIM16->ARR = period - 1U;

    TIM16->CCR1 = calculate_compare(period);

    timer->CR1 = cr1;

    return true;
}

bool bsp_buzzer_init(void) {
    if (initialized) {
        return true;
    }

    if ((RCC->APB2ENR & RCC_APB2ENR_TIM16EN) == 0U || !configure_frequency(BSP_BUZZER_FREQ_DEFAULT_HZ)) {
        return false;
    }

    /* Load the initially silent PWM registers
     * before connecting TIM16_CH1. */
    TIM16->CNT = 0U;

    TIM16->EGR = TIM_EGR_UG;

    TIM16->SR = ~TIM_SR_UIF;

    TIM16->CCER |= TIM_CCER_CC1E;
    TIM16->BDTR |= TIM_BDTR_MOE;
    TIM16->CR1 |= TIM_CR1_CEN;

    frequency_hz = BSP_BUZZER_FREQ_DEFAULT_HZ;
    volume_percent = BSP_BUZZER_VOLUME_DEFAULT_PERCENT;
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

bool bsp_buzzer_set_volume(uint8_t requested_percent) {
    if (!initialized || requested_percent > BSP_BUZZER_VOLUME_MAX_PERCENT) {
        return false;
    }

    volume_percent = requested_percent;

    uint32_t period = TIM16->ARR + 1U;

    TIM16->CCR1 = calculate_compare(period);

    return true;
}

bool bsp_buzzer_set_enabled(bool on) {
    if (!initialized) {
        return false;
    }

    enabled = on;

    uint32_t period = TIM16->ARR + 1U;

    TIM16->CCR1 = calculate_compare(period);

    return true;
}

bool bsp_buzzer_get_status(bsp_buzzer_status_t* status) {
    if (!initialized || status == NULL) {
        return false;
    }

    status->frequency_hz = frequency_hz;
    status->volume_percent = volume_percent;
    status->enabled = enabled;

    return true;
}
