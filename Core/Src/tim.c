/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
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

#include "tim.h"
#include "bsp_board_config.h"

/* Check the calculations before narrowing their results to peripheral registers. */
_Static_assert(BSP_BOARD_TIMER_US_TICK_HZ > 0U && BSP_BOARD_TIMER_US_TICK_HZ <= BSP_BOARD_TIMER_CLOCK_HZ,
               "The microsecond counter clock must be achievable");
_Static_assert(BSP_BOARD_TIMER_US_PSC <= UINT16_MAX && BSP_BOARD_TIMER_CLOCK_HZ % (BSP_BOARD_TIMER_US_PSC + 1U) == 0U
                   && BSP_BOARD_TIMER_CLOCK_HZ / (BSP_BOARD_TIMER_US_PSC + 1U) == BSP_BOARD_TIMER_US_TICK_HZ,
               "The microsecond prescaler must fit 16 bits and produce the exact counter clock");
_Static_assert(BSP_BOARD_BUZZER_INITIAL_RATE_HZ > 0U && BSP_BOARD_BUZZER_PSC <= UINT16_MAX
                   && BSP_BOARD_TIMER_CLOCK_HZ % ((BSP_BOARD_BUZZER_PSC + 1U) * BSP_BOARD_BUZZER_INITIAL_RATE_HZ) == 0U
                   && BSP_BOARD_BUZZER_INITIAL_ARR > 0U && BSP_BOARD_BUZZER_INITIAL_ARR <= UINT16_MAX,
               "The initial TIM16 period must be exact and fit its 16-bit counter");
_Static_assert(BSP_BOARD_HV_ADC_TARGET_RATE_HZ > 0U && BSP_BOARD_HV_ADC_PSC <= UINT16_MAX
                   && BSP_BOARD_TIMER_CLOCK_HZ % ((BSP_BOARD_HV_ADC_PSC + 1U) * BSP_BOARD_HV_ADC_TARGET_RATE_HZ) == 0U
                   && BSP_BOARD_HV_ADC_ARR > 0U && BSP_BOARD_HV_ADC_ARR <= UINT16_MAX
                   && BSP_BOARD_HV_ADC_RATE_HZ == BSP_BOARD_HV_ADC_TARGET_RATE_HZ,
               "The TIM3 ADC trigger period must be exact and fit its 16-bit counter");
_Static_assert(BSP_BOARD_HV_PWM_RATE_HZ > 0U && BSP_BOARD_HV_PWM_PSC <= UINT16_MAX
                   && BSP_BOARD_TIMER_CLOCK_HZ % ((BSP_BOARD_HV_PWM_PSC + 1U) * BSP_BOARD_HV_PWM_RATE_HZ) == 0U
                   && BSP_BOARD_HV_PWM_ARR > 0U && BSP_BOARD_HV_PWM_ARR <= UINT16_MAX,
               "The TIM17 PWM period must be exact and fit its 16-bit counter");
_Static_assert(BSP_BOARD_WS2812_DMA_RATE_HZ > 0U && BSP_BOARD_WS2812_PSC <= UINT16_MAX
                   && BSP_BOARD_TIMER_CLOCK_HZ % ((BSP_BOARD_WS2812_PSC + 1U) * BSP_BOARD_WS2812_DMA_RATE_HZ) == 0U
                   && BSP_BOARD_WS2812_ARR > 0U && BSP_BOARD_WS2812_ARR <= UINT32_MAX
                   && (BSP_BOARD_WS2812_ARR + 1U) % 2U == 0U,
               "The TIM2 DMA slot must fit its 32-bit counter and have an exact midpoint");

void platform_tim16_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
    (void)RCC->APB2ENR;
    TIM16->CR1 = TIM_CR1_ARPE;
    TIM16->PSC = (uint32_t)BSP_BOARD_BUZZER_PSC;
    TIM16->ARR = (uint32_t)BSP_BOARD_BUZZER_INITIAL_ARR;
    TIM16->RCR = 0U;
    TIM16->CCR1 = 0U;
    TIM16->CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM16->CCER = 0U;
    TIM16->BDTR = 0U;
    TIM16->EGR = TIM_EGR_UG;
    TIM16->SR = 0U;
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 12U)) | (2U << 12U);
    GPIOA->OTYPER &= ~(1U << 6U);
    GPIOA->PUPDR &= ~(3U << 12U);
    GPIOA->OSPEEDR &= ~(3U << 12U);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFU << 24U)) | (5U << 24U);
}

void platform_tim2_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    TIM2->CR1 = 0U;
    TIM2->CR2 = 0U;
    TIM2->SMCR = 0U;
    TIM2->DIER = 0U;
    TIM2->PSC = (uint32_t)BSP_BOARD_WS2812_PSC;
    TIM2->ARR = (uint32_t)BSP_BOARD_WS2812_ARR;
    TIM2->CCR3 = (uint32_t)BSP_BOARD_WS2812_CCR3;
    TIM2->CCMR2 = 0U;
    TIM2->CCER = 0U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    DMA1_Channel1->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1 | DMA_CCR_PL;
}

/* Use the board's ADC trigger rate; leave stopped until BSP HV is ready. */
void platform_tim3_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;
    TIM3->CR1 = TIM_CR1_ARPE;
    TIM3->CR2 = 0U; /* Do not emit a trigger when loading PSC/ARR below. */
    TIM3->SMCR = 0U;
    TIM3->DIER = 0U;
    TIM3->PSC = (uint32_t)BSP_BOARD_HV_ADC_PSC;
    TIM3->ARR = (uint32_t)BSP_BOARD_HV_ADC_ARR;
    TIM3->CNT = 0U;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    TIM3->CR2 = TIM_CR2_MMS_1; /* Update event drives TRGO. */
}

/* Board-configured HV PWM1, inverted polarity for the NPN gate driver. */
void platform_tim17_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
    (void)RCC->APB2ENR;
    TIM17->CR1 = TIM_CR1_ARPE;
    TIM17->CR2 = TIM_CR2_OIS1; /* HIGH idle keeps the MOSFET off. */
    TIM17->DIER = 0U;
    TIM17->PSC = (uint32_t)BSP_BOARD_HV_PWM_PSC;
    TIM17->ARR = (uint32_t)BSP_BOARD_HV_PWM_ARR;
    TIM17->RCR = 0U;
    TIM17->CNT = 0U;
    TIM17->CCR1 = 0U;
    TIM17->CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM17->BDTR = TIM_BDTR_OSSI | TIM_BDTR_BKP; /* MOE and AOE stay clear. */
    TIM17->CCER = TIM_CCER_CC1P | TIM_CCER_CC1E;
    TIM17->EGR = TIM_EGR_UG;
    TIM17->SR = 0U;
    /* Connect AF only after the timer can hold the output in its HIGH idle state. */
    GPIOA->BSRR = 1U << 7U;
    GPIOA->OTYPER &= ~(1U << 7U);
    GPIOA->PUPDR &= ~(3U << 14U);
    GPIOA->OSPEEDR |= 3U << 14U;
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFU << 28U)) | (5U << 28U);
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 14U)) | (2U << 14U);
}
