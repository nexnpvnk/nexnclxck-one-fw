#ifndef BSP_TIME_H
#define BSP_TIME_H
#include <stdint.h>
/* Wrapping monotonic milliseconds; serviced by the board's system tick. */
uint32_t bsp_time_ms(void);
#endif
