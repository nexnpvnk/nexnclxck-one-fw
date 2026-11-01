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

/* Check the calculations before narrowing their results to peripheral registers. */
_Static_assert(1000000UL <= PLATFORM_HCLK_HZ, "The microsecond counter clock must be achievable");
_Static_assert(PLATFORM_TIM_US_PSC <= TIM_PSC_PSC && PLATFORM_HCLK_HZ % (PLATFORM_TIM_US_PSC + 1U) == 0U
                   && PLATFORM_HCLK_HZ / (PLATFORM_TIM_US_PSC + 1U) == 1000000UL,
               "The microsecond prescaler must fit 16 bits and produce the exact counter clock");
_Static_assert(BSP_BOARD_BUZZER_INITIAL_RATE_HZ > 0U
                   && PLATFORM_HCLK_HZ % ((PLATFORM_TIM_US_PSC + 1U) * BSP_BOARD_BUZZER_INITIAL_RATE_HZ) == 0U
                   && PLATFORM_TIM16_ARR > 0U && PLATFORM_TIM16_ARR <= UINT16_MAX,
               "The initial TIM16 period must be exact and fit its 16-bit counter");
_Static_assert(BSP_BOARD_HV_ADC_TARGET_RATE_HZ > 0U
                   && PLATFORM_HCLK_HZ % ((PLATFORM_TIM_US_PSC + 1U) * BSP_BOARD_HV_ADC_TARGET_RATE_HZ) == 0U
                   && PLATFORM_TIM3_ARR > 0U && PLATFORM_TIM3_ARR <= UINT16_MAX
                   && TIMER_CALC_RATE_HZ(PLATFORM_HCLK_HZ, PLATFORM_TIM_US_PSC, PLATFORM_TIM3_ARR)
                          == BSP_BOARD_HV_ADC_TARGET_RATE_HZ,
               "The TIM3 ADC trigger period must be exact and fit its 16-bit counter");
_Static_assert(BSP_BOARD_HV_PWM_RATE_HZ > 0U && PLATFORM_HCLK_HZ % BSP_BOARD_HV_PWM_RATE_HZ == 0U
                   && PLATFORM_TIM17_ARR > 0U && PLATFORM_TIM17_ARR <= UINT16_MAX,
               "The TIM17 PWM period must be exact and fit its 16-bit counter");
_Static_assert((BSP_BOARD_WS2812_BIT_RATE_HZ * BSP_BOARD_WS2812_SYMBOLS_PER_BIT) > 0U
                   && PLATFORM_HCLK_HZ % (BSP_BOARD_WS2812_BIT_RATE_HZ * BSP_BOARD_WS2812_SYMBOLS_PER_BIT) == 0U
                   && PLATFORM_TIM2_ARR > 0U && PLATFORM_TIM2_ARR <= TIM_ARR_ARR && (PLATFORM_TIM2_ARR + 1U) % 2U == 0U,
               "The TIM2 DMA slot must fit its 32-bit counter and have an exact midpoint");

void platform_tim16_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
    (void)RCC->APB2ENR;
    TIM16->CR1 = TIM_CR1_ARPE;
    TIM16->PSC = (uint32_t)PLATFORM_TIM_US_PSC;
    TIM16->ARR = (uint32_t)PLATFORM_TIM16_ARR;
    TIM16->RCR = 0U;
    TIM16->CCR1 = 0U;
    TIM16->CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM16->CCER = 0U;
    TIM16->BDTR = 0U;
    TIM16->EGR = TIM_EGR_UG;
    TIM16->SR = 0U;
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER6) | GPIO_MODER_MODER6_1;
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT_6;
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR6;
    GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEEDR6;
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~GPIO_AFRL_AFSEL6) | (5U << GPIO_AFRL_AFSEL6_Pos);
}

void platform_tim2_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    TIM2->CR1 = 0U;
    TIM2->CR2 = 0U;
    TIM2->SMCR = 0U;
    TIM2->DIER = 0U;
    TIM2->PSC = 0U;
    TIM2->ARR = (uint32_t)PLATFORM_TIM2_ARR;
    TIM2->CCR3 = (uint32_t)PLATFORM_TIM2_CCR3;
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
    TIM3->PSC = (uint32_t)PLATFORM_TIM_US_PSC;
    TIM3->ARR = (uint32_t)PLATFORM_TIM3_ARR;
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
    TIM17->PSC = 0U;
    TIM17->ARR = (uint32_t)PLATFORM_TIM17_ARR;
    TIM17->RCR = 0U;
    TIM17->CNT = 0U;
    TIM17->CCR1 = 0U;
    TIM17->CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM17->BDTR = TIM_BDTR_OSSI | TIM_BDTR_BKP; /* MOE and AOE stay clear. */
    TIM17->CCER = TIM_CCER_CC1P | TIM_CCER_CC1E;
    TIM17->EGR = TIM_EGR_UG;
    TIM17->SR = 0U;
    /* Connect AF only after the timer can hold the output in its HIGH idle state. */
    GPIOA->BSRR = GPIO_BSRR_BS_7;
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT_7;
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR7;
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR7;
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~GPIO_AFRL_AFSEL7) | (5U << GPIO_AFRL_AFSEL7_Pos);
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER7) | GPIO_MODER_MODER7_1;
}
