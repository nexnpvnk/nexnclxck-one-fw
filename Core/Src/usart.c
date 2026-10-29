/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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

#include "usart.h"
#include "bsp_time.h"

void platform_usart1_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;
    GPIOA->MODER = (GPIOA->MODER & ~(0xFU << 18U)) | (0xAU << 18U);
    GPIOA->OTYPER &= ~((1U << 9U) | (1U << 10U));
    GPIOA->OSPEEDR |= 0xFU << 18U;
    GPIOA->PUPDR &= ~(0xFU << 18U);
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~0xFF0U) | 0x110U;
    SYSCFG->CFGR1 |= SYSCFG_CFGR1_USART1TX_DMA_RMP | SYSCFG_CFGR1_USART1RX_DMA_RMP;
    USART1->CR1 = 0U;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;
    USART1->BRR = (SystemCoreClock + 57600U) / 115200U;
    USART1->ICR = USART_ICR_TCCF | USART_ICR_IDLECF | USART_ICR_ORECF | USART_ICR_NCF | USART_ICR_FECF | USART_ICR_PECF;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    uint32_t start = bsp_time_ms();
    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) != (USART_ISR_TEACK | USART_ISR_REACK)) {
        if ((uint32_t)(bsp_time_ms() - start) >= 100U) {
            Error_Handler();
        }
    }
    NVIC_SetPriority(USART1_IRQn, 0U);
    NVIC_EnableIRQ(USART1_IRQn);
}
