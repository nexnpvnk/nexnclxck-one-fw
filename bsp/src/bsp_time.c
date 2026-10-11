#include "bsp_time.h"
#include "stm32f0xx_hal.h"

uint32_t bsp_time_ms(void) { return HAL_GetTick(); }
