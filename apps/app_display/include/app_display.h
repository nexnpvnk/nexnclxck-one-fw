#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include "nixie.h"

typedef struct {
    nixie_board_t board;
    nixie_timing_t timing;
    uint16_t brightness_pm;
    uint16_t cell_brightness_pm[NIXIE_MAX_ELEMENTS][NIXIE_MAX_GROUPS];
    uint32_t demo_step_ms;
} app_display_profile_t;

/* Default profile is in src/display_profile.c. Missing wiring keeps outputs off. */
bool app_display_init(void);
/* Foreground-only. The caller authorizes illumination when HV is ready.
 * Demo cycles 0..9; all additional groups/separators stay illuminated. */
void app_display_process(bool power_ready);
/* Validate every demo digit before publishing. Descriptor arrays/strings must
 * remain valid and immutable while selected. Invalid/BUSY leaves config unchanged. */
nixie_status_t app_display_configure(const app_display_profile_t* profile);
nixie_status_t app_display_set_brightness(uint16_t brightness_pm);
nixie_status_t app_display_set_cell_brightness(uint8_t element, uint8_t group, uint16_t brightness_pm);
nixie_status_t app_display_set_timing(const nixie_timing_t* timing);
nixie_issue_t app_display_last_issue(void);

#endif /* APP_DISPLAY_H */
