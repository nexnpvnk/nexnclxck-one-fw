
#include "app_light.h"

#include "bsp_time.h"
#include "bsp_ws2812.h"

#include <stddef.h>
#include <string.h>

#define APP_LIGHT_FRAME_MS 33U

static ledfx_rgb_t pixels[BSP_WS2812_MAX_LEDS];
static bsp_ws2812_rgb_t last_sent[BSP_WS2812_MAX_LEDS];

static ledfx_config_t config;

static uint32_t effect_started_ms;
static uint32_t last_render_ms;

static uint8_t led_count;
static uint8_t brightness;

static bool initialized;
static bool dirty;
static bool pending;
static bool have_sent;

bool app_light_init(uint8_t count) {
    if (initialized) {
        return count == led_count;
    }

    if (count == 0U || count > BSP_WS2812_MAX_LEDS || !bsp_ws2812_init()) {
        return false;
    }

    led_count = count;

    config = (ledfx_config_t){
        .effect = LEDFX_EFFECT_RAINBOW,
        .primary = {255U, 80U, 8U},
        .secondary = {0U, 32U, 255U},
        .speed = 128U,
        .seed = 0x2812U,
    };

    brightness = 32U;

    effect_started_ms = bsp_time_ms();
    last_render_ms = effect_started_ms;

    dirty = true;
    pending = false;
    have_sent = false;
    initialized = true;

    return true;
}

bool app_light_set_effect(ledfx_effect_t effect) {
    if (!initialized || effect >= LEDFX_EFFECT_COUNT) {
        return false;
    }

    if (config.effect != effect) {
        config.effect = effect;
        effect_started_ms = bsp_time_ms();
        dirty = true;
    }

    return true;
}

bool app_light_set_colors(ledfx_rgb_t primary, ledfx_rgb_t secondary) {
    if (!initialized) {
        return false;
    }

    config.primary = primary;
    config.secondary = secondary;

    dirty = true;
    return true;
}

bool app_light_set_speed(uint8_t speed) {
    if (!initialized) {
        return false;
    }

    if (config.speed != speed) {
        config.speed = speed;
        dirty = true;
    }

    return true;
}

bool app_light_set_brightness(uint8_t value) {
    if (!initialized) {
        return false;
    }

    if (brightness != value) {
        brightness = value;
        dirty = true;
    }

    return true;
}

void app_light_process(void) {
    if (!initialized) {
        return;
    }

    uint32_t now = bsp_time_ms();

    bool animated = config.effect != LEDFX_EFFECT_STATIC;

    if (dirty || (animated && (uint32_t)(now - last_render_ms) >= APP_LIGHT_FRAME_MS)) {

        if (!ledfx_render(&config, pixels, led_count, now - effect_started_ms)) {
            return;
        }

        /*
         * Application-level RGB processing and UX feedback
         * can be added here in the future.
         */

        last_render_ms = now;
        dirty = false;

        /* Compare the desired frame against the last
         * frame accepted by the physical transport. */
        pending = !have_sent;

        for (uint8_t i = 0U; i < led_count; ++i) {
            ledfx_rgb_t color = ledfx_color_scale(pixels[i], brightness);

            if (color.r != last_sent[i].r || color.g != last_sent[i].g || color.b != last_sent[i].b) {
                pending = true;
            }
        }
    }

    if (!pending || bsp_ws2812_is_busy()) {
        return;
    }

    /*
     * BSP copies the frame before returning.
     * A temporary stack buffer is therefore sufficient.
     */
    bsp_ws2812_rgb_t output[BSP_WS2812_MAX_LEDS];

    for (uint8_t i = 0U; i < led_count; ++i) {
        ledfx_rgb_t color = ledfx_color_scale(pixels[i], brightness);

        output[i] = (bsp_ws2812_rgb_t){
            .r = color.r,
            .g = color.g,
            .b = color.b,
        };
    }

    if (bsp_ws2812_show(output, led_count)) {
        memcpy(last_sent, output, (size_t)led_count * sizeof(output[0]));

        have_sent = true;
        pending = false;
    }
}
