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
    GPIOB->MODER =
        (GPIOB->MODER & ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7)) | GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1;
    GPIOB->OTYPER |= GPIO_OTYPER_OT_6 | GPIO_OTYPER_OT_7;
    GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR6 | GPIO_OSPEEDR_OSPEEDR7;
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR6 | GPIO_PUPDR_PUPDR7);
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(GPIO_AFRL_AFSEL6 | GPIO_AFRL_AFSEL7)) | (1U << GPIO_AFRL_AFSEL6_Pos)
                    | (1U << GPIO_AFRL_AFSEL7_Pos);
    I2C1->CR1 = 0U;
    I2C1->CR2 = 0U;
    I2C1->OAR1 = 0U;
    I2C1->OAR2 = 0U;
    I2C1->TIMINGR = (2U << I2C_TIMINGR_SCLDEL_Pos) | (29U << I2C_TIMINGR_SCLH_Pos) | (43U << I2C_TIMINGR_SCLL_Pos);
    DMA1_Channel2->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PL_1;
    DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_PL_1;
    /* Analog filter enabled, digital filter off, clock stretching enabled. */
    I2C1->CR1 = I2C_CR1_PE;
    NVIC_SetPriority(I2C1_IRQn, 0U);
    NVIC_EnableIRQ(I2C1_IRQn);
}
