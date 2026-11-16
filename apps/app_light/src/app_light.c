
#include "app_light.h"

#include "bsp_time.h"
#include "bsp_ws2812.h"

#include <stddef.h>
#include <string.h>

#define APP_LIGHT_FRAME_MS 33U

static ledfx_rgb_t pixels[BSP_WS2812_MAX_LEDS];
static bsp_ws2812_rgb_t last_sent[BSP_WS2812_MAX_LEDS];

static ledfx_config_t config;
static app_light_preview_t preview;

static uint64_t effect_elapsed_ms;
static uint32_t effect_updated_ms;
static uint32_t last_render_ms;

static uint8_t led_count;
static uint8_t brightness;

static bool initialized;
static bool dirty;
static bool pending;
static bool have_sent;
static uint32_t observed_ws2812_errors;

/**
 * \brief           Advance effect time using a wrap-safe difference of BSP timestamps
 * \param[in]       now: Current BSP timestamp in milliseconds
 */
static void prv_update_effect_time(uint32_t now) {
    effect_elapsed_ms += (uint32_t)(now - effect_updated_ms);
    effect_updated_ms = now;
}

bool app_light_init(uint8_t count) {
    if (initialized) {
        return count == led_count;
    }

    if (count == 0U || count > BSP_WS2812_MAX_LEDS || !bsp_ws2812_init()) {
        return false;
    }

    observed_ws2812_errors = bsp_ws2812_error_count();

    led_count = count;

    pending = false;
    have_sent = false;
    initialized = true;

    return app_light_reset();
}

bool app_light_reset(void) {
    if (!initialized) {
        return false;
    }

    config = (ledfx_config_t){
        .effect = LEDFX_EFFECT_STATIC,
        .primary = {255U, 80U, 8U},
        .secondary = {0U, 32U, 255U},
        .speed = 128U,
        .seed = 0x2812U,
    };

    brightness = 64U; /* About 25%. */

    effect_elapsed_ms = 0U;
    effect_updated_ms = bsp_time_ms();
    last_render_ms = effect_updated_ms;

    dirty = true;

    return true;
}

bool app_light_set_effect(ledfx_effect_t effect) {
    if (!initialized || effect >= LEDFX_EFFECT_COUNT) {
        return false;
    }

    if (config.effect != effect) {
        config.effect = effect;
        effect_elapsed_ms = 0U;
        effect_updated_ms = bsp_time_ms();
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

/**
 * \brief           Change effect speed while preserving its current phase
 * \param[in]       speed: Speed in the range `0..255`
 * \return          `true` on success, or `false` before initialization
 */
bool app_light_set_speed(uint8_t speed) {
    if (!initialized) {
        return false;
    }

    if (config.speed != speed) {
        uint32_t now = bsp_time_ms();
        uint32_t old_period_ms = ledfx_effect_period_ms(config.effect, config.speed);
        uint32_t new_period_ms = ledfx_effect_period_ms(config.effect, speed);

        prv_update_effect_time(now);
        if (old_period_ms > 0U) {
            /* Retain completed steps and fractional progress at the new speed. */
            effect_elapsed_ms = (effect_elapsed_ms * new_period_ms) / old_period_ms;
        }

        config.speed = speed;
        dirty = true;
    }

    return true;
}

/**
 * \brief           Change output brightness without changing the effect timeline
 * \param[in]       value: Brightness in the range `0..255`
 * \return          `true` on success, or `false` before initialization
 */
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

bool app_light_set_preview(app_light_preview_t value) {
    if (!initialized || (unsigned)value > APP_LIGHT_PREVIEW_SECONDARY) {
        return false;
    }
    if (preview != value) {
        preview = value;
        dirty = true;
    }
    return true;
}

bool app_light_get_status(app_light_status_t* status) {
    if (!initialized || status == NULL) {
        return false;
    }

    *status = (app_light_status_t){
        .effect = config.effect,
        .primary = config.primary,
        .secondary = config.secondary,
        .speed = config.speed,
        .brightness = brightness,
        .led_count = led_count,
    };

    return true;
}

void app_light_process(void) {
    if (!initialized) {
        return;
    }

    /* Service transfer timeouts even when a static frame has no changes.
     * Read errors afterward so an aborted transfer is retried. */
    bool transport_busy = bsp_ws2812_is_busy();
    uint32_t errors = bsp_ws2812_error_count();

    if (errors != observed_ws2812_errors) {
        observed_ws2812_errors = errors;

        /* Resubmit the frame after a failed physical transfer. */
        have_sent = false;
        dirty = true;
    }

    uint32_t now = bsp_time_ms();

    prv_update_effect_time(now);

    bool animated = preview == APP_LIGHT_PREVIEW_NONE && config.effect != LEDFX_EFFECT_STATIC;

    if (dirty || (animated && (uint32_t)(now - last_render_ms) >= APP_LIGHT_FRAME_MS)) {

        ledfx_config_t rendered = config;
        if (preview != APP_LIGHT_PREVIEW_NONE) {
            rendered.effect = LEDFX_EFFECT_STATIC;
            rendered.primary = preview == APP_LIGHT_PREVIEW_PRIMARY ? config.primary : config.secondary;
        }
        if (!ledfx_render(&rendered, pixels, led_count, effect_elapsed_ms)) {
            return;
        }

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

    if (!pending || transport_busy) {
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
