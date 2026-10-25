#ifndef BSP_WS2812_H
#define BSP_WS2812_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool bsp_ws2812_init(void);
bool bsp_ws2812_show_rgb(uint8_t r, uint8_t g, uint8_t b);
bool bsp_ws2812_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_WS2812_H */
