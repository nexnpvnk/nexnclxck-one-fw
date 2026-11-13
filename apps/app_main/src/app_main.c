#include "app_main.h"

#include "app_bme280.h"
#include "app_cli.h"
#include "app_clock.h"
#include "app_display.h"
#include "app_hv.h"
#include "app_input.h"
#include "app_light.h"
#include "app_sound.h"

#include "bsp_buzzer.h"
#include "bsp_console.h"
#include "bsp_i2c.h"
#include "bsp_time.h"

#include "shell.h"
#include "ulog.h"
#include "ulog_syslog.h"

#define TAG "main"

static void logger_prefix(ulog_event* event, char* buffer, size_t size) {
    static uint32_t message_id;
    (void)event;

    (void)snprintf(buffer, size, "+%lums | %03lu | ", (unsigned long)bsp_time_ms(), (unsigned long)message_id++);
}

static bool logger_init(void) {
    static const char* const topics[] = {
        "main",
        "input",
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
        printf("Warning: microlog initialization failed\n");
    } else {
        ulog_t(ULOG_SYSLOG_INFO, TAG, "Logging initialized");
    }

    if (!shell_init()) {
        while (1) {}
    }

    if (!app_input_init()) {
        ulog_t(ULOG_SYSLOG_ERR, TAG, "Button initialization failed");
    }

    /* Sound is optional; a failed buzzer must not prevent the clock from running. */
    (void)bsp_buzzer_init();

    if (!app_light_init(12U)) {
        ulog_t(ULOG_SYSLOG_ERR, TAG, "Lighting initialization failed");
    }

    if (!app_hv_init()) {
        ulog_t(ULOG_SYSLOG_ERR, TAG, "HV initialization failed; continuing");
    }

    if (!app_display_init()) {
        nixie_issue_t issue = app_display_last_issue();
        printf("Display disabled: %s, element=%u; edit app_display/src/display_profile.c\n",
               nixie_status_name(issue.status), (unsigned)issue.element);
    }

    if (!app_cli_init()) {
        while (1) {}
    }

    if (!bsp_i2c_init()) {
        ulog_t(ULOG_SYSLOG_CRIT, TAG, "Warning: I2C1 initialization failed; continuing");
    }

    (void)app_sound_beep(2800U, 10U, 100U);

    bool display_power_ready = false;
    while (1) {
        app_input_process();
        app_light_process();
        app_sound_process();
        app_bme280_process();
        app_hv_process();
        app_hv_status_t hv_status;
        /* Qualify initial HV startup once. Keep the display load connected
         * during ordinary regulation transients; OFF/FAULT revoke permission. */
        display_power_ready =
            app_hv_get_status(&hv_status) && hv_status.requested_enabled && hv_status.fault == APP_HV_FAULT_NONE
            && (hv_status.state == APP_HV_RUNNING || (display_power_ready && hv_status.state == APP_HV_STARTING));
        app_display_process(display_power_ready);
        bsp_i2c_process();
        shell_process();
        /* Handle cancellation and commands before the next periodic CLI output. */
        app_cli_process();
    }
}
