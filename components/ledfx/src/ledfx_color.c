
#include "ledfx.h"

static uint8_t scale8(uint8_t value, uint8_t scale) { return (uint8_t)(((uint32_t)value * scale + 127U) / 255U); }

ledfx_rgb_t ledfx_color_scale(ledfx_rgb_t color, uint8_t brightness) {
    return (ledfx_rgb_t){
        .r = scale8(color.r, brightness),
        .g = scale8(color.g, brightness),
        .b = scale8(color.b, brightness),
    };
}

static uint8_t blend8(uint8_t a, uint8_t b, uint8_t amount) {
    return (uint8_t)(((uint32_t)a * (255U - amount) + (uint32_t)b * amount + 127U) / 255U);
}

ledfx_rgb_t ledfx_color_blend(ledfx_rgb_t a, ledfx_rgb_t b, uint8_t amount) {
    return (ledfx_rgb_t){
        .r = blend8(a.r, b.r, amount),
        .g = blend8(a.g, b.g, amount),
        .b = blend8(a.b, b.b, amount),
    };
}

ledfx_rgb_t ledfx_hsv_to_rgb(ledfx_hsv_t hsv) {
    if (hsv.s == 0U) {
        return (ledfx_rgb_t){hsv.v, hsv.v, hsv.v};
    }

    uint32_t hue = (uint32_t)hsv.h * 6U;

    uint8_t region = (uint8_t)(hue >> 8U);
    uint8_t fraction = (uint8_t)(hue & 0xFFU);

    uint8_t p = scale8(hsv.v, (uint8_t)(255U - hsv.s));

    uint8_t q = scale8(hsv.v, (uint8_t)(255U - scale8(hsv.s, fraction)));

    uint8_t t = scale8(hsv.v, (uint8_t)(255U - scale8(hsv.s, (uint8_t)(255U - fraction))));

    switch (region) {
        case 0: return (ledfx_rgb_t){hsv.v, t, p};
        case 1: return (ledfx_rgb_t){q, hsv.v, p};
        case 2: return (ledfx_rgb_t){p, hsv.v, t};
        case 3: return (ledfx_rgb_t){p, q, hsv.v};
        case 4: return (ledfx_rgb_t){t, p, hsv.v};
        default: return (ledfx_rgb_t){hsv.v, p, q};
    }
}
