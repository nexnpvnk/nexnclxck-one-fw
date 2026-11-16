
#include "ledfx.h"

#include <stdint.h>

/* Map speed 0..255 to an effect-specific period. */
static uint32_t period_ms(uint8_t speed, uint32_t slow_ms, uint32_t fast_ms) {
    return slow_ms - ((slow_ms - fast_ms) * speed) / 255U;
}

/* Stateless pseudorandom mixing for deterministic flicker. */
static uint32_t hash32(uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    return value;
}

/**
 * \brief           Calculate a triangular waveform level from the current effect time
 * \param[in]       elapsed_ms: Time relative to the effect start
 * \param[in]       period: Nonzero cycle duration in milliseconds, at least `2`
 * \return          Waveform level in the range `0..255`
 */
static uint8_t triangle8(uint64_t elapsed_ms, uint32_t period) {
    uint32_t phase = (uint32_t)(elapsed_ms % period);
    uint32_t half = period / 2U;

    if (phase > half) {
        phase = period - phase;
    }

    return (uint8_t)((phase * 255U) / half);
}

/**
 * \brief           Get the duration of one effect cycle or discrete step
 * \param[in]       effect: Effect whose timing is requested
 * \param[in]       speed: Speed in the range `0..255`
 * \return          Duration in milliseconds, or `0` for static or invalid effects
 */
uint32_t ledfx_effect_period_ms(ledfx_effect_t effect, uint8_t speed) {
    switch (effect) {
        case LEDFX_EFFECT_BREATH:
            return period_ms(speed, 6000U, 1200U);
        case LEDFX_EFFECT_RAINBOW:
            return period_ms(speed, 12000U, 1500U);
        case LEDFX_EFFECT_FADE:
            return period_ms(speed, 8000U, 1200U);
        case LEDFX_EFFECT_FLICKER:
            return period_ms(speed, 240U, 35U);
        case LEDFX_EFFECT_COMET:
            return period_ms(speed, 350U, 65U);
        default:
            return 0U;
    }
}

/**
 * \brief           Render an effect without retaining state between calls
 * \param[in]       config: Effect configuration
 * \param[out]      pixels: RGB buffer written completely on success
 * \param[in]       count: Number of pixels to render
 * \param[in]       elapsed_ms: Time relative to the effect start
 * \return          `true` on success, or `false` for invalid arguments without changing the buffer
 */
bool ledfx_render(const ledfx_config_t* config, ledfx_rgb_t* pixels, size_t count, uint64_t elapsed_ms) {
    uint32_t period;

    if (config == NULL || pixels == NULL || count == 0U || config->effect >= LEDFX_EFFECT_COUNT) {
        return false;
    }

    period = ledfx_effect_period_ms(config->effect, config->speed);

    switch (config->effect) {
        case LEDFX_EFFECT_STATIC:
            for (size_t i = 0; i < count; ++i) {
                pixels[i] = config->primary;
            }
            break;

        case LEDFX_EFFECT_BREATH: {
            uint8_t amplitude = triangle8(elapsed_ms, period);
            uint8_t level = (uint8_t)(8U + ((uint32_t)amplitude * 247U) / 255U);

            ledfx_rgb_t color = ledfx_color_scale(config->primary, level);

            for (size_t i = 0; i < count; ++i) {
                pixels[i] = color;
            }
            break;
        }

        case LEDFX_EFFECT_RAINBOW: {
            uint32_t phase = (uint32_t)(elapsed_ms % period);
            uint8_t offset = (uint8_t)((phase * 256U) / period);

            for (size_t i = 0; i < count; ++i) {
                uint8_t hue = (uint8_t)(((uint32_t)i * 256U) / count + offset);

                pixels[i] = ledfx_hsv_to_rgb((ledfx_hsv_t){hue, 255U, 255U});
            }
            break;
        }

        case LEDFX_EFFECT_FADE: {
            uint8_t amount = triangle8(elapsed_ms, period);

            ledfx_rgb_t color = ledfx_color_blend(config->primary, config->secondary, amount);

            for (size_t i = 0; i < count; ++i) {
                pixels[i] = color;
            }
            break;
        }

        case LEDFX_EFFECT_FLICKER: {
            uint32_t step = (uint32_t)(elapsed_ms / period);

            for (size_t i = 0; i < count; ++i) {
                uint32_t random = hash32(config->seed ^ (step * 0x9E3779B9U) ^ ((uint32_t)i * 0x85EBCA6BU));

                uint8_t level = (uint8_t)(155U + ((random >> 24U) * 100U) / 255U);

                pixels[i] = ledfx_color_scale(config->primary, level);
            }
            break;
        }

        case LEDFX_EFFECT_COMET: {
            uint32_t head = (uint32_t)((elapsed_ms / period) % ((uint32_t)count + 4U));

            for (size_t i = 0; i < count; ++i) {
                uint8_t level = 0U;

                if (head >= i && head - i < 4U) {
                    level = (uint8_t)(255U - (head - i) * 65U);
                }

                pixels[i] = ledfx_color_scale(config->primary, level);
            }
            break;
        }

        default:
            return false;
    }

    return true;
}

const char* ledfx_effect_name(ledfx_effect_t effect) {
    static const char* const names[] = {
        "static",
        "breath",
        "rainbow",
        "fade",
        "flicker",
        "comet",
    };

    return (unsigned)effect < LEDFX_EFFECT_COUNT ? names[effect] : "unknown";
}
