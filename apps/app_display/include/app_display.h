#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include "nixie.h"

#define APP_DISPLAY_REFRESH_RATE_MIN_HZ 1U
#define APP_DISPLAY_REFRESH_RATE_MAX_HZ 400U

typedef struct {
    nixie_board_t board;
    nixie_timing_t timing;
    uint16_t brightness_pm;
    uint16_t anode_brightness_pm[NIXIE_MAX_ANODES]; /* One PWM factor per ANODE_n. */
    uint32_t demo_step_ms;
    /* Element indices in circular fill order. Count=0 disables this pattern. */
    const uint8_t* demo_point_order;
    uint8_t demo_point_count;
} app_display_profile_t;

typedef struct {
    uint16_t brightness_pm;
    uint16_t refresh_rate_hz; /* Accepted target; publication occurs at a frame boundary. */
    uint16_t blanking_time_us;
    uint32_t missed_slots; /* Cumulative BSP counter, including late PWM completion. */
    bool running;
} app_display_status_t;

/* Default profile is in src/display_profile.c. Missing wiring keeps outputs off. */
bool app_display_init(void);
/* Foreground-only. The caller authorizes illumination when HV is ready.
 * Numeric elements cycle through their symbols with a one-position offset.
 * Configured points fill in circular order; other auxiliary elements stay lit. */
void app_display_process(bool power_ready);
/* Validate every demo symbol with all pattern points visible before publishing.
 * Descriptor arrays/strings, including demo_point_order, must
 * remain valid and immutable while selected. Invalid/BUSY leaves config unchanged. */
nixie_status_t app_display_configure(const app_display_profile_t* profile);
/* Foreground-only. Update parameters in one validated publication, preserving
 * anode brightness and the current demo position. */
nixie_status_t app_display_set_parameters(uint16_t brightness_pm, uint16_t refresh_rate_hz, uint16_t blanking_time_us);
bool app_display_get_status(app_display_status_t* status);
nixie_status_t app_display_set_brightness(uint16_t brightness_pm);
nixie_status_t app_display_set_anode_brightness(uint8_t anode, uint16_t brightness_pm);
nixie_status_t app_display_set_timing(const nixie_timing_t* timing);
nixie_issue_t app_display_last_issue(void);

#endif /* APP_DISPLAY_H */
