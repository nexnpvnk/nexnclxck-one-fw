#ifndef BSP_TIME_H
#define BSP_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wrapping monotonic milliseconds; serviced by the board's system tick. */
uint32_t bsp_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TIME_H */
