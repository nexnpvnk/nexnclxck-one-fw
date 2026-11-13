
#ifndef APP_LIGHT_H
#define APP_LIGHT_H

#include <stdbool.h>
#include <stdint.h>

#include "ledfx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ledfx_effect_t effect;
    ledfx_rgb_t primary;
    ledfx_rgb_t secondary;
    uint8_t speed;
    uint8_t brightness;
    uint8_t led_count;
} app_light_status_t;

/* Initialize the lighting application for the active LED count. */
bool app_light_init(uint8_t led_count);

/* Foreground-only. Restore startup parameters and restart the effect phase.
 * Preserve LED count and transport state. Returns false if not initialized. */
bool app_light_reset(void);

/* Call regularly from the foreground superloop. */
void app_light_process(void);

bool app_light_set_effect(ledfx_effect_t effect);
bool app_light_set_colors(ledfx_rgb_t primary, ledfx_rgb_t secondary);
bool app_light_set_speed(uint8_t speed);
bool app_light_set_brightness(uint8_t brightness);

/* Foreground-only. Read requested parameters, independent of DMA completion.
 * Returns false for NULL or before successful initialization. */
bool app_light_get_status(app_light_status_t* status);

#ifdef __cplusplus
}
#endif

#endif /* APP_LIGHT_H */
