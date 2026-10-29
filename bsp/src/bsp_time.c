#include "bsp_time.h"

static volatile uint32_t milliseconds;

uint32_t bsp_time_ms(void) { return milliseconds; }

void bsp_time_tick(void) { ++milliseconds; }
