/**
  ******************************************************************************
  * @file    tim.h
  * @brief   This file contains all the function prototypes for
  *          the tim.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#ifndef __TIM_H__
#define __TIM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_board_config.h"
#include "main.h"
#include "platform_config.h"
#include "timer_calc.h"

/* Register values for the existing board initialization, derived from BSP rates.
 * TIM3/TIM16 use a 1 us counter; TIM2/TIM17 run without a prescaler. */
#define PLATFORM_TIM_US_PSC TIMER_CALC_PSC(PLATFORM_HCLK_HZ, 1000000UL)
#define PLATFORM_TIM16_ARR  TIMER_CALC_ARR(PLATFORM_HCLK_HZ, PLATFORM_TIM_US_PSC, BSP_BOARD_BUZZER_INITIAL_RATE_HZ)
#define PLATFORM_TIM3_ARR   TIMER_CALC_ARR(PLATFORM_HCLK_HZ, PLATFORM_TIM_US_PSC, BSP_BOARD_HV_ADC_TARGET_RATE_HZ)
#define PLATFORM_TIM17_ARR  TIMER_CALC_ARR(PLATFORM_HCLK_HZ, 0U, BSP_BOARD_HV_PWM_RATE_HZ)
#define PLATFORM_TIM2_ARR                                                                                              \
    TIMER_CALC_ARR(PLATFORM_HCLK_HZ, 0U, (BSP_BOARD_WS2812_BIT_RATE_HZ * BSP_BOARD_WS2812_SYMBOLS_PER_BIT))
#define PLATFORM_TIM2_CCR3 ((PLATFORM_TIM2_ARR + 1U) / 2U)

void platform_tim2_init(void);
void platform_tim3_init(void);
void platform_tim16_init(void);
void platform_tim17_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __TIM_H__ */
