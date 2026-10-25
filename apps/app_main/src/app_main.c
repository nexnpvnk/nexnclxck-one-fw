#include "app_main.h"

#include "app_bme280.h"
#include "app_cli.h"
#include "app_clock.h"
#include "app_sound.h"

#include "bsp_buzzer.h"
#include "bsp_console.h"
#include "bsp_i2c.h"
#include "bsp_storage.h"
#include "bsp_time.h"

#include "shell.h"
#include "ulog.h"
#include "ulog_syslog.h"

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
        "main", "rtc", "i2c", "bme280", "sound", "storage",
    };

    if (ulog_syslog_enable() != ULOG_STATUS_OK) {
        return false;
    }

    if (ulog_prefix_set_fn(logger_prefix) != ULOG_STATUS_OK) {
        return false;
    }

    if (ulog_output_level_set(ULOG_OUTPUT_STDOUT, ULOG_SYSLOG_INFO) != ULOG_STATUS_OK) {
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
        ulog_t(ULOG_SYSLOG_INFO, "main", "Logging initialized");
    }

    if (!shell_init()) {
        while (1) {}
    }

    /* Keep the console available for diagnostics if storage initialization fails. */
    (void)bsp_storage_init();

    /* Sound is optional; a failed buzzer must not prevent the clock from running. */
    (void)bsp_buzzer_init();

    if (!app_cli_init()) {
        while (1) {}
    }

    if (!bsp_i2c_init()) {
        printf("Warning: I2C1 initialization failed; continuing\r\n");
    }

    (void)app_sound_beep(2800U, 10U, 100U);

    while (1) {
        app_sound_process();
        bsp_i2c_process();
        app_bme280_process();
        shell_process();
        app_cli_process();
    }
}
