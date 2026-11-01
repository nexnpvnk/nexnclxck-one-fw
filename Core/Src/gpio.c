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
            gpio->MODER |= GPIO_MODER_MODER0 << (pin * GPIO_MODER_MODER1_Pos);
            gpio->PUPDR &= ~(GPIO_PUPDR_PUPDR0 << (pin * GPIO_PUPDR_PUPDR1_Pos));
        }
    }
}

void platform_gpio_init(void) {
    RCC->AHBENR |=
        RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN | RCC_AHBENR_GPIODEN | RCC_AHBENR_GPIOFEN;
    (void)RCC->AHBENR;
    analog_pins(GPIOF, GPIO_ODR_0 | GPIO_ODR_1 | GPIO_ODR_4 | GPIO_ODR_5 | GPIO_ODR_6 | GPIO_ODR_7);
    analog_pins(GPIOA, GPIO_ODR_0 | GPIO_ODR_1 | GPIO_ODR_3 | GPIO_ODR_4 | GPIO_ODR_5 | GPIO_ODR_8 | GPIO_ODR_11
                           | GPIO_ODR_12 | GPIO_ODR_15);
    analog_pins(GPIOB, GPIO_ODR_3 | GPIO_ODR_4 | GPIO_ODR_5 | GPIO_ODR_8 | GPIO_ODR_9 | GPIO_ODR_12 | GPIO_ODR_13
                           | GPIO_ODR_14 | GPIO_ODR_15);
    analog_pins(GPIOC, GPIO_ODR_6 | GPIO_ODR_7 | GPIO_ODR_8 | GPIO_ODR_9 | GPIO_ODR_10 | GPIO_ODR_11);

    /* Set output latches before switching pins from input to output. */
    GPIOC->BSRR = GPIO_BSRR_BS_5 | GPIO_BSRR_BS_12 | GPIO_BSRR_BR_4;
    GPIOC->MODER = (GPIOC->MODER & ~(GPIO_MODER_MODER4 | GPIO_MODER_MODER5 | GPIO_MODER_MODER12)) | GPIO_MODER_MODER4_0
                   | GPIO_MODER_MODER5_0 | GPIO_MODER_MODER12_0;
    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT_4 | GPIO_OTYPER_OT_5 | GPIO_OTYPER_OT_12);
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR4 | GPIO_PUPDR_PUPDR5 | GPIO_PUPDR_PUPDR12);
    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEEDR4 | GPIO_OSPEEDR_OSPEEDR5 | GPIO_OSPEEDR_OSPEEDR12);
    GPIOB->BSRR = GPIO_BSRR_BR_0 | GPIO_BSRR_BR_1 | GPIO_BSRR_BR_2 | GPIO_BSRR_BR_10 | GPIO_BSRR_BR_11;
    GPIOB->MODER =
        (GPIOB->MODER
         & ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1 | GPIO_MODER_MODER2 | GPIO_MODER_MODER10 | GPIO_MODER_MODER11))
        | GPIO_MODER_MODER0_0 | GPIO_MODER_MODER1_0 | GPIO_MODER_MODER2_0 | GPIO_MODER_MODER10_0 | GPIO_MODER_MODER11_0;
    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT_0 | GPIO_OTYPER_OT_1 | GPIO_OTYPER_OT_2 | GPIO_OTYPER_OT_10 | GPIO_OTYPER_OT_11);
    GPIOB->PUPDR &=
        ~(GPIO_PUPDR_PUPDR0 | GPIO_PUPDR_PUPDR1 | GPIO_PUPDR_PUPDR2 | GPIO_PUPDR_PUPDR10 | GPIO_PUPDR_PUPDR11);
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEEDR0 | GPIO_OSPEEDR_OSPEEDR1 | GPIO_OSPEEDR_OSPEEDR2 | GPIO_OSPEEDR_OSPEEDR10
                        | GPIO_OSPEEDR_OSPEEDR11);

    /* HIGH keeps the inverting HV gate driver off before TIM17 takes over. */
    GPIOA->BSRR = GPIO_BSRR_BS_7;
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT_7;
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR7;
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR7;
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER7) | GPIO_MODER_MODER7_0;
    GPIOC->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1 | GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(GPIO_PUPDR_PUPDR0 | GPIO_PUPDR_PUPDR1 | GPIO_PUPDR_PUPDR2 | GPIO_PUPDR_PUPDR3))
                   | GPIO_PUPDR_PUPDR0_0 | GPIO_PUPDR_PUPDR1_0 | GPIO_PUPDR_PUPDR2_0 | GPIO_PUPDR_PUPDR3_0;
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0]
                         & ~(SYSCFG_EXTICR1_EXTI0 | SYSCFG_EXTICR1_EXTI1 | SYSCFG_EXTICR1_EXTI2 | SYSCFG_EXTICR1_EXTI3))
                        | SYSCFG_EXTICR1_EXTI0_PC | SYSCFG_EXTICR1_EXTI1_PC | SYSCFG_EXTICR1_EXTI2_PC
                        | SYSCFG_EXTICR1_EXTI3_PC;
    EXTI->RTSR |= EXTI_RTSR_TR0 | EXTI_RTSR_TR1 | EXTI_RTSR_TR2 | EXTI_RTSR_TR3;
    EXTI->FTSR |= EXTI_FTSR_TR0 | EXTI_FTSR_TR1 | EXTI_FTSR_TR2 | EXTI_FTSR_TR3;
    EXTI->PR = EXTI_PR_PR0 | EXTI_PR_PR1 | EXTI_PR_PR2 | EXTI_PR_PR3;
    EXTI->IMR |= EXTI_IMR_MR0 | EXTI_IMR_MR1 | EXTI_IMR_MR2 | EXTI_IMR_MR3;

    /* Preserve the board's open-drain WS2812 output. */
    GPIOD->BSRR = GPIO_BSRR_BR_2;
    GPIOD->MODER = (GPIOD->MODER & ~GPIO_MODER_MODER2) | GPIO_MODER_MODER2_0;
    GPIOD->OTYPER |= GPIO_OTYPER_OT_2;
    GPIOD->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR2;
    GPIOD->PUPDR &= ~GPIO_PUPDR_PUPDR2;
    NVIC_SetPriority(EXTI0_1_IRQn, 3U);
    NVIC_EnableIRQ(EXTI0_1_IRQn);
    NVIC_SetPriority(EXTI2_3_IRQn, 3U);
    NVIC_EnableIRQ(EXTI2_3_IRQn);
}
