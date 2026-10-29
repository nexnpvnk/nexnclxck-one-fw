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

void platform_tim16_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
    (void)RCC->APB2ENR;
    TIM16->CR1 = TIM_CR1_ARPE;
    TIM16->PSC = 47U;
    TIM16->ARR = 499U;
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
    TIM2->PSC = 0U;
    TIM2->ARR = 19U;
    TIM2->CCR3 = 10U;
    TIM2->CCMR2 = 0U;
    TIM2->CCER = 0U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    DMA1_Channel1->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1 | DMA_CCR_PL;
}

/* ADC trigger: 48 MHz / 48 / 500 = 2 kHz. Leave stopped until BSP HV is ready. */
void platform_tim3_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;
    TIM3->CR1 = TIM_CR1_ARPE;
    TIM3->CR2 = 0U; /* Do not emit a trigger when loading PSC/ARR below. */
    TIM3->SMCR = 0U;
    TIM3->DIER = 0U;
    TIM3->PSC = 47U;
    TIM3->ARR = 499U;
    TIM3->CNT = 0U;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    TIM3->CR2 = TIM_CR2_MMS_1; /* Update event drives TRGO. */
}

/* HV: 50 kHz PWM1, inverted polarity for the board's NPN gate driver. */
void platform_tim17_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
    (void)RCC->APB2ENR;
    TIM17->CR1 = TIM_CR1_ARPE;
    TIM17->CR2 = TIM_CR2_OIS1; /* HIGH idle keeps the MOSFET off. */
    TIM17->DIER = 0U;
    TIM17->PSC = 0U;
    TIM17->ARR = 959U;
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
