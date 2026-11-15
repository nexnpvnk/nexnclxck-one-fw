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
 * Short FN cycles effects on RELEASE; long FN opens brightness adjustment.
 * LEFT/RIGHT adjust 0..100% in one-percent steps, with accelerating hold repeat.
 * Holding LEFT/RIGHT in adjustment is silent. OK/short FN or 10 seconds of
 * inactivity finish adjustment; changes apply immediately. */
void app_ui_handle_input(app_ui_key_t key, app_ui_event_t event);

#endif /* APP_UI_H */
