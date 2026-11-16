#include "app_input.h"

#include "app_ui.h"

#include "bsp_buttons.h"
#include "bsp_time.h"

#include "lwbtn/lwbtn.h"
#include "ulog.h"
#include "ulog_syslog.h"

#define APP_INPUT_PERIOD_MS     5U
#define APP_INPUT_LONG_PRESS_MS 1000U

static lwbtn_btn_t buttons[BSP_BUTTON_COUNT];
static bool long_press_reported[BSP_BUTTON_COUNT];
static uint32_t last_process_ms;

/* Translate board button identities into UI keys; GPIO and lwbtn stay private to input. */
static const app_ui_key_t ui_keys[] = {
    [BSP_BUTTON_RIGHT] = APP_UI_KEY_RIGHT,
    [BSP_BUTTON_OK] = APP_UI_KEY_OK,
    [BSP_BUTTON_LEFT] = APP_UI_KEY_LEFT,
    [BSP_BUTTON_LIGHT] = APP_UI_KEY_FN,
};

_Static_assert(sizeof(ui_keys) / sizeof(ui_keys[0]) == BSP_BUTTON_COUNT, "Map every board button to a UI key");

/**
 * \brief           Read the physical button state during foreground processing.
 * \param[in]       group: Button group, unused.
 * \param[in]       button: Button from the configured `buttons` array.
 * \return          `1` when pressed, otherwise `0`.
 */
static uint8_t prv_button_get_state(lwbtn_t* group, lwbtn_btn_t* button) {
    size_t id = (size_t)(button - buttons);

    (void)group;

    if (id >= BSP_BUTTON_COUNT) {
        return 0U;
    }

    return (uint8_t)((bsp_buttons_get_pressed_mask() >> id) & 1U);
}

static void button_event(lwbtn_t* group, lwbtn_btn_t* button, lwbtn_evt_t event) {
    (void)group;

    size_t id = (size_t)(button - buttons);

    if (id >= BSP_BUTTON_COUNT) {
        return;
    }

    switch (event) {
        case LWBTN_EVT_ONPRESS:
            long_press_reported[id] = false;

            app_ui_handle_input(ui_keys[id], APP_UI_EVENT_PRESS);
            break;

        case LWBTN_EVT_ONRELEASE:
            app_ui_handle_input(ui_keys[id], APP_UI_EVENT_RELEASE);
            break;

        case LWBTN_EVT_ONCLICK:
            break;

        case LWBTN_EVT_KEEPALIVE:
            if (!long_press_reported[id] && lwbtn_keepalive_get_count(button) >=
                                                lwbtn_keepalive_get_count_for_time(button, APP_INPUT_LONG_PRESS_MS)) {

                long_press_reported[id] = true;

                app_ui_handle_input(ui_keys[id], APP_UI_EVENT_LONG_PRESS);
            }
            break;

        default:
            break;
    }
}

bool app_input_init(void) {
    last_process_ms = bsp_time_ms();

    return lwbtn_init(buttons, BSP_BUTTON_COUNT, prv_button_get_state, button_event) != 0U;
}

void app_input_process(void) {
    uint32_t now = bsp_time_ms();

    if ((uint32_t)(now - last_process_ms) < APP_INPUT_PERIOD_MS) {
        return;
    }

    last_process_ms = now;

    (void)lwbtn_process(now);
}
