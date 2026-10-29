/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

#include "gpio.h"

static void analog_pins(GPIO_TypeDef* gpio, uint32_t pins) {
    for (uint32_t pin = 0U; pin < 16U; ++pin) {
        if ((pins & (1U << pin)) != 0U) {
            gpio->MODER |= 3U << (pin * 2U);
            gpio->PUPDR &= ~(3U << (pin * 2U));
        }
    }
}

void platform_gpio_init(void) {
    RCC->AHBENR |=
        RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN | RCC_AHBENR_GPIODEN | RCC_AHBENR_GPIOFEN;
    (void)RCC->AHBENR;
    analog_pins(GPIOF, 0x00F3U);
    analog_pins(GPIOA, 0x993BU);
    analog_pins(GPIOB, 0xF338U);
    analog_pins(GPIOC, 0x0FC0U);

    /* Set output latches before switching pins from input to output. */
    GPIOC->BSRR = (1U << 5U) | (1U << 12U) | (1U << 20U);
    GPIOC->MODER = (GPIOC->MODER & ~((3U << 8U) | (3U << 10U) | (3U << 24U))) | (1U << 8U) | (1U << 10U) | (1U << 24U);
    GPIOC->OTYPER &= ~0x1030U;
    GPIOC->PUPDR &= ~((3U << 8U) | (3U << 10U) | (3U << 24U));
    GPIOC->OSPEEDR &= ~((3U << 8U) | (3U << 10U) | (3U << 24U));
    GPIOB->BSRR = 0x0C07U << 16U;
    GPIOB->MODER = (GPIOB->MODER & ~((0x3FU << 0U) | (0xFU << 20U))) | 0x15U | (0x5U << 20U);
    GPIOB->OTYPER &= ~0x0C07U;
    GPIOB->PUPDR &= ~((0x3FU << 0U) | (0xFU << 20U));
    GPIOB->OSPEEDR &= ~((0x3FU << 0U) | (0xFU << 20U));

    /* HIGH keeps the inverting HV gate driver off before TIM17 takes over. */
    GPIOA->BSRR = 1U << 7U;
    GPIOA->OTYPER &= ~(1U << 7U);
    GPIOA->PUPDR &= ~(3U << 14U);
    GPIOA->OSPEEDR |= 3U << 14U;
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 14U)) | (1U << 14U);
    GPIOC->MODER &= ~0xFFU;
    GPIOC->PUPDR = (GPIOC->PUPDR & ~0xFFU) | 0x55U;
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~0xFFFFU) | 0x2222U;
    EXTI->RTSR |= 0x0FU;
    EXTI->FTSR |= 0x0FU;
    EXTI->PR = 0x0FU;
    EXTI->IMR |= 0x0FU;

    /* Preserve the board's open-drain WS2812 output. */
    GPIOD->BSRR = 1U << 18U;
    GPIOD->MODER = (GPIOD->MODER & ~(3U << 4U)) | (1U << 4U);
    GPIOD->OTYPER |= 1U << 2U;
    GPIOD->OSPEEDR |= 3U << 4U;
    GPIOD->PUPDR &= ~(3U << 4U);
    NVIC_SetPriority(EXTI0_1_IRQn, 3U);
    NVIC_EnableIRQ(EXTI0_1_IRQn);
    NVIC_SetPriority(EXTI2_3_IRQn, 3U);
    NVIC_EnableIRQ(EXTI2_3_IRQn);
}
