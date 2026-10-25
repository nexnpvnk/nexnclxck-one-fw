#include "app_input.h"

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

static const char* const button_names[BSP_BUTTON_COUNT] = {
    "RIGHT",
    "OK",
    "LEFT",
    "LIGHT",
};

static void button_event(lwbtn_t* group, lwbtn_btn_t* button, lwbtn_evt_t event) {
    (void)group;

    size_t id = (size_t)(button - buttons);

    if (id >= BSP_BUTTON_COUNT) {
        return;
    }

    const char* name = button_names[id];

    switch (event) {
        case LWBTN_EVT_ONPRESS:
            long_press_reported[id] = false;

            ulog_t(ULOG_SYSLOG_DEBUG, "input", "%s pressed\r", name);
            break;

        case LWBTN_EVT_ONRELEASE: ulog_t(ULOG_SYSLOG_DEBUG, "input", "%s released\r", name); break;

        case LWBTN_EVT_ONCLICK:
            ulog_t(ULOG_SYSLOG_DEBUG, "input", "%s click x%u\r", name, (unsigned)lwbtn_click_get_count(button));
            break;

        case LWBTN_EVT_KEEPALIVE:
            if (!long_press_reported[id]
                && lwbtn_keepalive_get_count(button)
                       >= lwbtn_keepalive_get_count_for_time(button, APP_INPUT_LONG_PRESS_MS)) {

                long_press_reported[id] = true;

                ulog_t(ULOG_SYSLOG_DEBUG, "input", "%s long press\r", name);
            }
            break;

        default: break;
    }
}

bool app_input_init(void) {
    bsp_buttons_init();

    last_process_ms = bsp_time_ms();

    return lwbtn_init(buttons, BSP_BUTTON_COUNT, NULL, button_event) != 0U;
}

void app_input_process(void) {
    uint32_t now = bsp_time_ms();

    if ((uint32_t)(now - last_process_ms) < APP_INPUT_PERIOD_MS) {
        return;
    }

    last_process_ms = now;

    uint8_t states = bsp_buttons_get_pressed_mask();

    for (uint32_t i = 0; i < BSP_BUTTON_COUNT; ++i) {
        (void)lwbtn_set_btn_state(&buttons[i], (uint8_t)((states >> i) & 1U));
    }

    (void)lwbtn_process(now);
}
