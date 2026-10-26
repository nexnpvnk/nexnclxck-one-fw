
#ifndef APP_LIGHT_H
#define APP_LIGHT_H

#include <stdbool.h>
#include <stdint.h>

#include "ledfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the lighting application for the active LED count. */
bool app_light_init(uint8_t led_count);

/* Call regularly from the foreground superloop. */
void app_light_process(void);

bool app_light_set_effect(ledfx_effect_t effect);
bool app_light_set_colors(ledfx_rgb_t primary, ledfx_rgb_t secondary);
bool app_light_set_speed(uint8_t speed);
bool app_light_set_brightness(uint8_t brightness);

#ifdef __cplusplus
}
#endif

#endif /* APP_LIGHT_H */
