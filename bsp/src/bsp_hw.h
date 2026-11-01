#ifndef BSP_HW_H
#define BSP_HW_H

#include "bsp_board_config.h"
#include "main.h"
#include "platform_config.h"
#include "timer_calc.h"

/* Private STM32F051 board bindings, startup entry points and register values.
 * Derived from BSP device rates and the platform system clock.
 * TIM3/TIM16 use a 1 us counter; TIM2/TIM17 run without a prescaler. */
#define BSP_TIM_US_PSC TIMER_CALC_PSC(PLATFORM_HCLK_HZ, 1000000UL)
#define BSP_TIM16_ARR  TIMER_CALC_ARR(PLATFORM_HCLK_HZ, BSP_TIM_US_PSC, BSP_BOARD_BUZZER_INITIAL_RATE_HZ)
#define BSP_TIM3_ARR   TIMER_CALC_ARR(PLATFORM_HCLK_HZ, BSP_TIM_US_PSC, BSP_BOARD_HV_ADC_TARGET_RATE_HZ)
#define BSP_TIM17_ARR  TIMER_CALC_ARR(PLATFORM_HCLK_HZ, 0U, BSP_BOARD_HV_PWM_RATE_HZ)
#define BSP_TIM2_ARR                                                                                                   \
    TIMER_CALC_ARR(PLATFORM_HCLK_HZ, 0U, (BSP_BOARD_WS2812_BIT_RATE_HZ * BSP_BOARD_WS2812_SYMBOLS_PER_BIT))
#define BSP_TIM2_CCR3 ((BSP_TIM2_ARR + 1U) / 2U)

void bsp_rtc_hw_init(void);
void bsp_console_hw_init(void);
void bsp_i2c_hw_init(void);
void bsp_buzzer_hw_init(void);
void bsp_ws2812_hw_init(void);
void bsp_hv_hw_init(void);
void bsp_nixie_hw_init(void);

#endif /* BSP_HW_H */
