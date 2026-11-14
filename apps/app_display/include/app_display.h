#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include "nixie.h"

#define APP_DISPLAY_REFRESH_RATE_MIN_HZ 1U
#define APP_DISPLAY_REFRESH_RATE_MAX_HZ 400U

typedef enum {
    APP_DISPLAY_POINT_LEFT_TOP = 0,
    APP_DISPLAY_POINT_LEFT_BOTTOM,
    APP_DISPLAY_POINT_RIGHT_TOP,
    APP_DISPLAY_POINT_RIGHT_BOTTOM,
    APP_DISPLAY_POINT_COUNT,
} app_display_point_t;

typedef struct {
    uint8_t digit_count; /* 4 or 6; digits[] lists board element indices from left to right. */
    uint8_t digits[6];
    /* Board element indices, NIXIE_CHANNEL_UNASSIGNED for absent points.
     * On a four-digit board LEFT_TOP/BOTTOM describe the central colon. */
    uint8_t points[APP_DISPLAY_POINT_COUNT];
} app_display_layout_t;

typedef struct {
    nixie_board_t board;
    /* NULL selects manual frames. Automatic presentation uses this layout;
     * points and digits must have separate anodes. */
    const app_display_layout_t* layout;
    nixie_timing_t timing;
    uint16_t brightness_pm;
    uint16_t anode_brightness_pm[NIXIE_MAX_ANODES]; /* One PWM factor per ANODE_n. */
} app_display_profile_t;

typedef struct {
    uint16_t brightness_pm;
    uint16_t refresh_rate_hz; /* Accepted target; publication occurs at a frame boundary. */
    uint16_t blanking_time_us;
    bool running;
    bool separators_on; /* RTC gate of lit separator slots in the active frame. */
} app_display_status_t;

/* Default profile is in src/display_profile.c. Starts with every element blank. */
bool app_display_init(void);
/* Foreground-only. Authorize illumination when HV is ready and update automatic
 * time/date/sensor screens when layout is configured. Time separators follow
 * RTC via TIM1; other points are steady. The time-set marker does not gate
 * display. A transient RTC snapshot failure retains the accepted frame. */
void app_display_process(bool power_ready);
/* Foreground-only. -1/1 selects previous/next available screen for 10 seconds,
 * then returns to time and resumes the automatic cycle. Other values are ignored. */
void app_display_step(int8_t direction);
/* Select a profile and blank all elements. Descriptor arrays/strings must remain
 * valid and immutable while selected. Invalid/BUSY leaves the current state intact. */
nixie_status_t app_display_configure(const app_display_profile_t* profile);
/* Copy one symbol ID per board element. Count must equal board.element_count.
 * Foreground-only; invalid/BUSY leaves the current frame intact. Automatic clock
 * presentation owns the frame while layout is configured. Internal dot overlays
 * are cleared by this raw-symbol operation. */
nixie_status_t app_display_present(const uint8_t* symbols, uint8_t count);
/* Foreground-only. Update parameters in one validated publication, preserving
 * anode brightness and the current symbols. Brightness applies to all slots. */
nixie_status_t app_display_set_parameters(uint16_t brightness_pm, uint16_t refresh_rate_hz, uint16_t blanking_time_us);
bool app_display_get_status(app_display_status_t* status);
nixie_issue_t app_display_last_issue(void);

#endif /* APP_DISPLAY_H */
