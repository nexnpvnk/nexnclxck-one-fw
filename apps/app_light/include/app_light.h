
#ifndef APP_LIGHT_H
#define APP_LIGHT_H

#include <stdbool.h>
#include <stdint.h>

#include "ledfx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_LIGHT_PREVIEW_NONE = 0,
    APP_LIGHT_PREVIEW_PRIMARY,
    APP_LIGHT_PREVIEW_SECONDARY,
} app_light_preview_t;

typedef struct {
    ledfx_effect_t effect;
    ledfx_rgb_t primary;
    ledfx_rgb_t secondary;
    uint8_t speed;      /* 0..255; higher is faster. */
    uint8_t brightness; /* 0..255; 0 turns lighting off. */
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
/* Speed and brightness use the full 0..255 range in the application and renderer. */
bool app_light_set_speed(uint8_t speed);
bool app_light_set_brightness(uint8_t brightness);

/* Foreground-only. Temporarily show a color slot as static at current brightness.
 * Does not change the selected effect or colors. NONE resumes that effect. */
bool app_light_set_preview(app_light_preview_t preview);

/* Foreground-only. Read requested parameters, independent of DMA completion.
 * Returns false for NULL or before successful initialization. */
bool app_light_get_status(app_light_status_t* status);

#ifdef __cplusplus
}
#endif

#endif /* APP_LIGHT_H */
