
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

typedef enum {
    BSP_WS2812_ERROR_NONE = 0,
    BSP_WS2812_ERROR_DMA,
    BSP_WS2812_ERROR_DEADLINE,
    BSP_WS2812_ERROR_TIMEOUT,
    BSP_WS2812_ERROR_START,
} bsp_ws2812_error_t;

typedef struct {
    uint32_t completed;
    uint32_t failed;
    uint32_t missed_deadlines;
    uint32_t timeouts;
    bsp_ws2812_error_t last_error;
} bsp_ws2812_stats_t;

bool bsp_ws2812_init(void);

/* Nonblocking. Copies the complete RGB frame.
 * Returns false if the driver is busy or arguments are invalid. */
bool bsp_ws2812_show(const bsp_ws2812_rgb_t* colors, size_t count);

/* Includes the post-frame reset/latch interval. */
bool bsp_ws2812_is_busy(void);

/* Atomically readable failure counter, including start errors. */
uint32_t bsp_ws2812_error_count(void);

/* Diagnostic snapshot; call from foreground context. */
bool bsp_ws2812_get_stats(bsp_ws2812_stats_t* stats);

/* Platform dispatch for DMA1_Channel1_IRQHandler. */
void bsp_ws2812_dma_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_WS2812_H */
