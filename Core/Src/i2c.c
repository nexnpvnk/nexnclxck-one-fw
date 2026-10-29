/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
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

#include "i2c.h"

void platform_i2c1_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    GPIOB->MODER = (GPIOB->MODER & ~(0xFU << 12U)) | (0xAU << 12U);
    GPIOB->OTYPER |= 0xC0U;
    GPIOB->OSPEEDR |= 0xFU << 12U;
    GPIOB->PUPDR &= ~(0xFU << 12U);
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~0xFF000000U) | 0x11000000U;
    I2C1->CR1 = 0U;
    I2C1->CR2 = 0U;
    I2C1->OAR1 = 0U;
    I2C1->OAR2 = 0U;
    I2C1->TIMINGR = 0x00201D2BU;
    DMA1_Channel2->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PL_1;
    DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_PL_1;
    /* Analog filter enabled, digital filter off, clock stretching enabled. */
    I2C1->CR1 = I2C_CR1_PE;
    NVIC_SetPriority(I2C1_IRQn, 0U);
    NVIC_EnableIRQ(I2C1_IRQn);
}
