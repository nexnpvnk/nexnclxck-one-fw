#ifndef UI2C_TEST_STM32F0XX_H
#define UI2C_TEST_STM32F0XX_H
/* Use ST's real register layouts/masks; replace only core intrinsics and MMIO. */
#include <stdint.h>
#define __CORE_CM0_H_GENERIC
#define __CMSIS_GENERIC
#define __IO volatile
#define __I  volatile
#define __O  volatile
#include "stm32f051x8.h"
extern DMA_TypeDef test_dma;
extern DMA_Channel_TypeDef test_channels[5];
extern uint32_t test_primask;
#undef DMA1
#undef DMA1_Channel1
#undef DMA1_Channel2
#undef DMA1_Channel3
#undef DMA1_Channel4
#undef DMA1_Channel5
#define DMA1          (&test_dma)
#define DMA1_Channel1 (&test_channels[0])
#define DMA1_Channel2 (&test_channels[1])
#define DMA1_Channel3 (&test_channels[2])
#define DMA1_Channel4 (&test_channels[3])
#define DMA1_Channel5 (&test_channels[4])

static inline uint32_t __get_PRIMASK(void) { return test_primask; }

static inline void __disable_irq(void) { test_primask = 1U; }

static inline void __set_PRIMASK(uint32_t key) { test_primask = key; }

static inline void __DMB(void) {}

static inline void __DSB(void) {}
#endif
