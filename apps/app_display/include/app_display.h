#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include "nixie.h"

#define APP_DISPLAY_REFRESH_RATE_MIN_HZ 1U
#define APP_DISPLAY_REFRESH_RATE_MAX_HZ 400U

typedef enum {
    APP_DISPLAY_CLOCK_UNUSED = 0,
    APP_DISPLAY_CLOCK_HOUR_TENS,
    APP_DISPLAY_CLOCK_HOUR_UNITS,
    APP_DISPLAY_CLOCK_MINUTE_TENS,
    APP_DISPLAY_CLOCK_MINUTE_UNITS,
    APP_DISPLAY_CLOCK_SECOND_TENS,
    APP_DISPLAY_CLOCK_SECOND_UNITS,
    APP_DISPLAY_CLOCK_SEPARATOR,
} app_display_clock_role_t;

typedef struct {
    nixie_board_t board;
    /* One role per board element; NULL keeps explicit frame control only. */
    const app_display_clock_role_t* clock_roles;
    nixie_timing_t timing;
    uint16_t brightness_pm;
    uint16_t anode_brightness_pm[NIXIE_MAX_ANODES]; /* One PWM factor per ANODE_n. */
} app_display_profile_t;

typedef struct {
    uint16_t brightness_pm;
    uint16_t refresh_rate_hz; /* Accepted target; publication occurs at a frame boundary. */
    uint16_t blanking_time_us;
    uint32_t missed_slots; /* Cumulative BSP counter, including late PWM completion. */
    bool running;
} app_display_status_t;

/* Default profile is in src/display_profile.c. Starts with every element blank. */
bool app_display_init(void);
/* Foreground-only. Authorize illumination when HV is ready and update the clock
 * frame when clock_roles is configured. Invalid time/unready RTC is blanked;
 * a failed snapshot retains the previous frame for retry. */
void app_display_process(bool power_ready);
/* Select a profile and blank all elements. Descriptor arrays/strings must remain
 * valid and immutable while selected. Invalid/BUSY leaves the current state intact. */
nixie_status_t app_display_configure(const app_display_profile_t* profile);
/* Copy one symbol ID per board element. Count must equal board.element_count.
 * Foreground-only; invalid/BUSY leaves the current frame intact. Automatic clock
 * presentation owns the frame while clock_roles is configured. */
nixie_status_t app_display_present(const uint8_t* symbols, uint8_t count);
/* Foreground-only. Update parameters in one validated publication, preserving
 * anode brightness and the current symbols. */
nixie_status_t app_display_set_parameters(uint16_t brightness_pm, uint16_t refresh_rate_hz, uint16_t blanking_time_us);
bool app_display_get_status(app_display_status_t* status);
nixie_issue_t app_display_last_issue(void);

#endif /* APP_DISPLAY_H */
