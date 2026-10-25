#include "app_main.h"

#include "app_bme280.h"
#include "app_cli.h"
#include "app_clock.h"
#include "app_input.h"
#include "app_sound.h"

#include "bsp_buzzer.h"
#include "bsp_console.h"
#include "bsp_i2c.h"
#include "bsp_storage.h"
#include "bsp_time.h"

#include "shell.h"
#include "ulog.h"
#include "ulog_syslog.h"

#include "bsp_ws2812.h"

#define TAG "main"

static void logger_prefix(ulog_event* event, char* buffer, size_t size) {
    static uint32_t message_id;
    app_clock_datetime_t datetime;

    (void)event;

    uint32_t id = message_id++;

    if (app_clock_get(&datetime)) {
        (void)snprintf(buffer, size, "%02u:%02u:%02u | MsgID:%03lu | ", (unsigned)datetime.hour,
                       (unsigned)datetime.minute, (unsigned)datetime.second, (unsigned long)id);
    } else {
        (void)snprintf(buffer, size, "+%lums | MsgID:%03lu | ", (unsigned long)bsp_time_ms(), (unsigned long)id);
    }
}

static bool logger_init(void) {
    static const char* const topics[] = {
        "main", "rtc", "i2c", "bme280", "sound", "storage", "input",
    };

    if (ulog_syslog_enable() != ULOG_STATUS_OK) {
        return false;
    }

    if (ulog_prefix_set_fn(logger_prefix) != ULOG_STATUS_OK) {
        return false;
    }

    if (ulog_output_level_set(ULOG_OUTPUT_STDOUT, ULOG_SYSLOG_DEBUG) != ULOG_STATUS_OK) {
        return false;
    }

    for (size_t i = 0; i < sizeof(topics) / sizeof(topics[0]); ++i) {
        if (ulog_topic_add(topics[i], ULOG_OUTPUT_ALL, ULOG_SYSLOG_DEBUG) == ULOG_TOPIC_ID_INVALID) {
            return false;
        }
    }

    return true;
}

void app_main(void) {
    /* Bring up diagnostics before optional peripherals. */
    if (!bsp_console_init()) {
        while (1) {}
    }

    if (!logger_init()) {
        printf("Warning: microlog initialization failed\r\n");
    } else {
        ulog_t(ULOG_SYSLOG_INFO, TAG, "Logging initialized\r");
    }

    if (!shell_init()) {
        while (1) {}
    }

    if (!app_input_init()) {
        ulog_t(ULOG_SYSLOG_ERR, TAG, "Button initialization failed\r");
    }

    /* Keep the console available for diagnostics if storage initialization fails. */
    (void)bsp_storage_init();

    /* Sound is optional; a failed buzzer must not prevent the clock from running. */
    (void)bsp_buzzer_init();

    bool ws2812_ready = bsp_ws2812_init();

    if (!ws2812_ready) {
        ulog_t(ULOG_SYSLOG_ERR, TAG, "WS2812 initialization failed\r");
    }

    if (!app_cli_init()) {
        while (1) {}
    }

    if (!bsp_i2c_init()) {
        ulog_t(ULOG_SYSLOG_CRIT, TAG, "Warning: I2C1 initialization failed; continuing\r");
    }

    (void)app_sound_beep(2800U, 10U, 100U);

    static const uint8_t test_colors[][3] = {
        {32U, 0U, 0U},
        {0U, 32U, 0U},
        {0U, 0U, 32U},
    };

    uint32_t last_led_ms = bsp_time_ms();
    uint8_t led_phase = 0U;

    while (1) {
        uint32_t now = bsp_time_ms();

        if (ws2812_ready && (uint32_t)(now - last_led_ms) >= 1000U) {

            if (bsp_ws2812_show_rgb(test_colors[led_phase][0], test_colors[led_phase][1], test_colors[led_phase][2])) {

                last_led_ms = now;

                led_phase++;

                if (led_phase >= 3U) {
                    led_phase = 0U;
                }
            }
        }

        app_input_process();
        app_sound_process();
        bsp_i2c_process();
        app_bme280_process();
        shell_process();
        app_cli_process();
    }
}
