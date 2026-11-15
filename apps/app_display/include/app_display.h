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
    uint8_t digit_count; /* Digit elements in physical left-to-right order; at most six. */
    uint8_t digits[6];
    /* Board element indices, NIXIE_CHANNEL_UNASSIGNED for absent points.
     * Four-digit panels use LEFT_TOP/BOTTOM for the central colon. */
    uint8_t points[APP_DISPLAY_POINT_COUNT];
} app_display_layout_t;

typedef struct {
    nixie_board_t board;
    const app_display_layout_t* layout; /* Physical geometry; NULL for a panel without a UI layout. */
    nixie_timing_t timing;
    uint16_t brightness_pm;
    uint16_t anode_brightness_pm[NIXIE_MAX_ANODES]; /* One PWM factor per ANODE_n. */
    uint8_t led_count; /* Physical WS2812 population of the upper board. */
} app_display_profile_t;

typedef struct {
    uint8_t symbols[NIXIE_MAX_ELEMENTS]; /* One symbol ID per logical board element. */
    uint16_t dot_elements;               /* Add an internal dot to the selected elements. */
    /* Select logical elements gated by the board RTC phase. Gating applies to
     * the whole shared anode; the application must group its elements accordingly. */
    uint16_t blink_elements;
    uint8_t element_count;
} app_display_frame_t;

typedef struct {
    uint16_t brightness_pm;
    uint16_t refresh_rate_hz; /* Accepted target; publication occurs at a frame boundary. */
    uint16_t blanking_time_us;
    bool running;
    bool separators_on; /* RTC gate of lit separator slots in the active frame. */
} app_display_status_t;

/* Foreground-only. Initialize once with the immutable profile in src/display_profile.c.
 * Starts with every element blank; repeated successful initialization is a no-op. */
bool app_display_init(void);
/* Foreground-only. Apply the HV permission and return whether illumination is
 * enabled. Does not choose or render content. Emergency shutdown remains in BSP. */
bool app_display_process(bool power_ready);
/* Immutable upper-board description and startup defaults, available before init.
 * Valid for the program lifetime; does not imply display readiness.
 * Read live brightness/timing through get_status(). */
const app_display_profile_t* app_display_profile(void);
/* Foreground-only. Copy logical content; element_count must match the board.
 * Invalid/BUSY leaves the accepted frame intact. Identical content is a no-op.
 * Brightness and scan timing are owned by the display profile, not by the frame. */
nixie_status_t app_display_present(const app_display_frame_t* frame);
/* Foreground-only. Update parameters in one validated publication, preserving
 * anode brightness and the current symbols. Brightness applies to all slots. */
nixie_status_t app_display_set_parameters(uint16_t brightness_pm, uint16_t refresh_rate_hz, uint16_t blanking_time_us);
bool app_display_get_status(app_display_status_t* status);
nixie_issue_t app_display_last_issue(void);

#endif /* APP_DISPLAY_H */
