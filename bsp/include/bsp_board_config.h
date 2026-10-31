#ifndef BSP_BOARD_CONFIG_H
#define BSP_BOARD_CONFIG_H

#include "timer_calc.h"

/* STM32F051: HSI / 2 feeds PLL; SystemClock_Config keeps AHB and APB at /1. */
#define BSP_BOARD_HSI_HZ                 8000000UL
#define BSP_BOARD_PLL_MULTIPLIER         12UL
#define BSP_BOARD_HCLK_HZ                ((BSP_BOARD_HSI_HZ / 2U) * BSP_BOARD_PLL_MULTIPLIER)
#define BSP_BOARD_TIMER_CLOCK_HZ         BSP_BOARD_HCLK_HZ

/* One-microsecond counter resolution for TIM3 and TIM16. */
#define BSP_BOARD_TIMER_US_TICK_HZ       1000000UL
#define BSP_BOARD_TIMER_US_PSC           TIMER_CALC_PSC(BSP_BOARD_TIMER_CLOCK_HZ, BSP_BOARD_TIMER_US_TICK_HZ)

#define BSP_BOARD_BUZZER_INITIAL_RATE_HZ 2000UL
#define BSP_BOARD_BUZZER_PSC             BSP_BOARD_TIMER_US_PSC
#define BSP_BOARD_BUZZER_INITIAL_ARR                                                                                   \
    TIMER_CALC_ARR(BSP_BOARD_TIMER_CLOCK_HZ, BSP_BOARD_BUZZER_PSC, BSP_BOARD_BUZZER_INITIAL_RATE_HZ)

/* ADC trigger rate. The exported rate is derived from the configured registers. */
#define BSP_BOARD_HV_ADC_TARGET_RATE_HZ 2000UL
#define BSP_BOARD_HV_ADC_PSC            BSP_BOARD_TIMER_US_PSC
#define BSP_BOARD_HV_ADC_ARR                                                                                           \
    TIMER_CALC_ARR(BSP_BOARD_TIMER_CLOCK_HZ, BSP_BOARD_HV_ADC_PSC, BSP_BOARD_HV_ADC_TARGET_RATE_HZ)
#define BSP_BOARD_HV_ADC_RATE_HZ                                                                                       \
    TIMER_CALC_RATE_HZ(BSP_BOARD_TIMER_CLOCK_HZ, BSP_BOARD_HV_ADC_PSC, BSP_BOARD_HV_ADC_ARR)

/* TIM17 runs at the full timer clock for maximum PWM duty resolution. */
#define BSP_BOARD_HV_PWM_RATE_HZ 20000UL
#define BSP_BOARD_HV_PWM_PSC     TIMER_CALC_PSC(BSP_BOARD_TIMER_CLOCK_HZ, BSP_BOARD_TIMER_CLOCK_HZ)
#define BSP_BOARD_HV_PWM_ARR TIMER_CALC_ARR(BSP_BOARD_TIMER_CLOCK_HZ, BSP_BOARD_HV_PWM_PSC, BSP_BOARD_HV_PWM_RATE_HZ)

/* Each WS2812 bit uses three BSRR writes; CC3 requests DMA halfway through a slot. */
#define BSP_BOARD_WS2812_BIT_RATE_HZ     800000UL
#define BSP_BOARD_WS2812_SYMBOLS_PER_BIT 3UL
#define BSP_BOARD_WS2812_DMA_RATE_HZ     (BSP_BOARD_WS2812_BIT_RATE_HZ * BSP_BOARD_WS2812_SYMBOLS_PER_BIT)
#define BSP_BOARD_WS2812_PSC             TIMER_CALC_PSC(BSP_BOARD_TIMER_CLOCK_HZ, BSP_BOARD_TIMER_CLOCK_HZ)
#define BSP_BOARD_WS2812_ARR                                                                                           \
    TIMER_CALC_ARR(BSP_BOARD_TIMER_CLOCK_HZ, BSP_BOARD_WS2812_PSC, BSP_BOARD_WS2812_DMA_RATE_HZ)
#define BSP_BOARD_WS2812_CCR3 ((BSP_BOARD_WS2812_ARR + 1U) / 2U)

#endif /* BSP_BOARD_CONFIG_H */
