
#ifndef LEDFX_H
#define LEDFX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ledfx_rgb_t;

typedef struct {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} ledfx_hsv_t;

typedef enum {
    LEDFX_EFFECT_STATIC = 0,
    LEDFX_EFFECT_BREATH,
    LEDFX_EFFECT_RAINBOW,
    LEDFX_EFFECT_FADE,
    LEDFX_EFFECT_FLICKER,
    LEDFX_EFFECT_COMET,
    LEDFX_EFFECT_COUNT
} ledfx_effect_t;

typedef struct {
    ledfx_effect_t effect;
    ledfx_rgb_t primary;
    ledfx_rgb_t secondary;
    uint8_t speed;
    uint32_t seed;
} ledfx_config_t;

/*
 * Render an effect into the caller-owned RGB buffer.
 *
 * - Entire buffer is written on success.
 * - elapsed_ms is relative to the effect start.
 * - No state is retained between calls.
 * - Invalid arguments leave the buffer unchanged.
 */
bool ledfx_render(const ledfx_config_t* config, ledfx_rgb_t* pixels, size_t count, uint32_t elapsed_ms);

ledfx_rgb_t ledfx_hsv_to_rgb(ledfx_hsv_t hsv);

ledfx_rgb_t ledfx_color_blend(ledfx_rgb_t a, ledfx_rgb_t b, uint8_t amount);

ledfx_rgb_t ledfx_color_scale(ledfx_rgb_t color, uint8_t brightness);

const char* ledfx_effect_name(ledfx_effect_t effect);

#ifdef __cplusplus
}
#endif

#endif /* LEDFX_H */
