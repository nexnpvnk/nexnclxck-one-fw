
#ifndef BSP_WS2812_H
#define BSP_WS2812_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_WS2812_MAX_LEDS 12U

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} bsp_ws2812_rgb_t;

bool bsp_ws2812_init(void);

/* Nonblocking. Copies the complete RGB frame.
 * Returns false if the driver is busy or arguments are invalid. */
bool bsp_ws2812_show(const bsp_ws2812_rgb_t* colors, size_t count);

/* Includes the post-frame reset/latch interval. */
bool bsp_ws2812_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_WS2812_H */
