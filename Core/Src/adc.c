/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
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

#include "adc.h"

void platform_adc_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    (void)RCC->APB2ENR;
    /* Configuration only. Calibration and conversion start belong to BSP HV. */
    RCC->APB2RSTR |= RCC_APB2RSTR_ADC1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_ADC1RST;
    GPIOA->MODER |= 3U << 4U;
    GPIOA->PUPDR &= ~(3U << 4U);
    ADC1->IER = 0U;
    /* 12-bit, right aligned, one channel, rising TIM3 TRGO, overwrite on OVR. */
    ADC1->CFGR1 = ADC_CFGR1_EXTSEL_0 | ADC_CFGR1_EXTSEL_1 | ADC_CFGR1_EXTEN_0 | ADC_CFGR1_OVRMOD;
    ADC1->CFGR2 = 0U;          /* Asynchronous HSI14 / 1. */
    ADC1->SMPR = ADC_SMPR_SMP; /* 239.5 cycles, unchanged from the board setup. */
    ADC1->CHSELR = ADC_CHSELR_CHSEL2;
    ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_EOSMP | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR | ADC_ISR_AWD;
    NVIC_ClearPendingIRQ(ADC1_COMP_IRQn);
    NVIC_SetPriority(ADC1_COMP_IRQn, 1U);
    NVIC_EnableIRQ(ADC1_COMP_IRQn);
}
