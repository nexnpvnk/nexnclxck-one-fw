/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

#include "main.h"
#include "adc.h"
#include "app_main.h"
#include "bsp_hv.h"
#include "bsp_time.h"
#include "dma.h"
#include "gpio.h"
#include "i2c.h"
#include "rtc.h"
#include "tim.h"
#include "usart.h"

static void wait_bits(volatile uint32_t* reg, uint32_t mask, uint32_t value, uint32_t timeout) {
    uint32_t start = bsp_time_ms();
    while ((*reg & mask) != value) {
        if ((uint32_t)(bsp_time_ms() - start) >= timeout) {
            Error_Handler();
        }
    }
}

void SystemClock_Config(void) {
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGCOMPEN;
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_DBP;

    RCC->CR |= RCC_CR_HSION;
    wait_bits(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY, 100U);
    /* Keep the dedicated ADC clock and its factory-default trim. */
    RCC->CR2 = (RCC->CR2 & ~RCC_CR2_HSI14TRIM) | (16U << RCC_CR2_HSI14TRIM_Pos) | RCC_CR2_HSI14ON;
    wait_bits(&RCC->CR2, RCC_CR2_HSI14RDY, RCC_CR2_HSI14RDY, 100U);
    RCC->CFGR &= ~RCC_CFGR_SW;
    wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI, 100U);
    RCC->CR &= ~RCC_CR_PLLON;
    wait_bits(&RCC->CR, RCC_CR_PLLRDY, 0U, 100U);
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL)) | RCC_CFGR_PLLMUL12;
    RCC->CFGR2 &= ~RCC_CFGR2_PREDIV;
    RCC->CR |= RCC_CR_PLLON;
    wait_bits(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY, 100U);
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL, 100U);
    SystemCoreClockUpdate();
    if (SystemCoreClock != 48000000U || SysTick_Config(SystemCoreClock / 1000U) != 0U) {
        Error_Handler();
    }
    NVIC_SetPriority(SysTick_IRQn, 3U);

    /* Preserve the calendar on ordinary resets. Only a source change requires BDRST. */
    uint32_t rtc_source = RCC->BDCR & RCC_BDCR_RTCSEL;
    if (rtc_source != 0U && rtc_source != RCC_BDCR_RTCSEL_0) {
        RCC->BDCR |= RCC_BDCR_BDRST;
        RCC->BDCR &= ~RCC_BDCR_BDRST;
    }
    RCC->BDCR |= RCC_BDCR_LSEDRV | RCC_BDCR_LSEON;
    wait_bits(&RCC->BDCR, RCC_BDCR_LSERDY, RCC_BDCR_LSERDY, 5000U);
    RCC->BDCR |= RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCEN;
    /* USART1 from PCLK, I2C1 from HSI. */
    RCC->CFGR3 &= ~(RCC_CFGR3_USART1SW | RCC_CFGR3_I2C1SW);
}

int main(void) {
    SystemCoreClockUpdate();
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) {
        Error_Handler();
    }
    NVIC_SetPriority(SysTick_IRQn, 3U);
    SystemClock_Config();
    platform_gpio_init();
    platform_dma_init();
    platform_rtc_init();
    platform_usart1_init();
    platform_i2c1_init();
    platform_tim16_init();
    platform_tim2_init();
    platform_adc_init();
    platform_tim3_init();
    platform_tim17_init();
    app_main();
    Error_Handler();
}

void Error_Handler(void) {
    __disable_irq();
    /* Also callable before peripheral initialization and from fault context. */
    bsp_hv_shutdown();
    __DSB();
    for (;;) {}
}
