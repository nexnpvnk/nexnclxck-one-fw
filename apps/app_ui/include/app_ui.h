#ifndef APP_UI_H
#define APP_UI_H

#include "nixie.h"

typedef enum {
    APP_UI_KEY_LEFT = 0,
    APP_UI_KEY_RIGHT,
    APP_UI_KEY_OK,
    APP_UI_KEY_FN,
    APP_UI_KEY_COUNT,
} app_ui_key_t;

typedef enum {
    APP_UI_EVENT_PRESS = 0,
    APP_UI_EVENT_RELEASE,
    APP_UI_EVENT_LONG_PRESS,
} app_ui_event_t;

/* Foreground-only. Call after app_display_init(); geometry comes from the fixed
 * upper-board profile. A failed initialization disables UI; issue may be NULL. */
nixie_status_t app_ui_init(nixie_issue_t* issue);
/* Foreground-only. can_render is the illumination state from app_display_process().
 * False suppresses publication, preserving UI state and input handling. UI owns
 * display content; a transient RTC read failure retains the accepted frame. */
void app_ui_process(bool can_render);
/* Foreground-only. Input reports PRESS, optionally one LONG_PRESS, then RELEASE.
 * UI chooses the action and sound. Events are handled even when rendering is
 * unavailable; invalid keys/events and calls before initialization are ignored.
 * Short FN cycles effects outside the editor. Long FN enters at setting 1.
 * Inside, short FN on RELEASE selects the next setting: 1 brightness, 2 speed,
 * 3 primary, 4 secondary, then 1 again. Show its ID left and its value right.
 * LEFT/RIGHT edit brightness/speed (0..255) or palette entries (1..16), with silent
 * accelerating hold repeat. OK or 10 seconds of inactivity finish.
 * Changes apply immediately; color preview ends when leaving color settings. */
void app_ui_handle_input(app_ui_key_t key, app_ui_event_t event);

#endif /* APP_UI_H */
