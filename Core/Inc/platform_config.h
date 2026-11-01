#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

#include "stm32f0xx.h"

/* MCU clock configuration. AHB and APB run at /1, so timer clocks equal HCLK.
 * HSI frequency is not exported by the CMSIS device header. */
#define PLATFORM_HSI_HZ     8000000UL
#define PLATFORM_PLL_CONFIG RCC_CFGR_PLLMUL12
#define PLATFORM_HCLK_HZ    ((PLATFORM_HSI_HZ / 2U) * ((PLATFORM_PLL_CONFIG >> RCC_CFGR_PLLMUL_Pos) + 2U))

#endif /* PLATFORM_CONFIG_H */
