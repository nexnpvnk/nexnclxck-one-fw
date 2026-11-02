#include "app_display.h"

#include "bsp_nixie.h"
#include "display_profile.h"

#include <stddef.h>
#include <string.h>

static app_display_profile_t selected;
static uint8_t current_symbols[NIXIE_MAX_ELEMENTS];
static nixie_plan_t scratch_plan;
static nixie_issue_t last_issue = {NIXIE_NOT_READY, UINT8_MAX};
static bool initialized;
static bool enabled;

static nixie_status_t publish_configuration(const app_display_profile_t* profile, const uint8_t* symbols) {
    if (profile == NULL || symbols == NULL || profile->board.element_count == 0U
        || profile->board.element_count > NIXIE_MAX_ELEMENTS) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX};
        return last_issue.status;
    }
    if (profile->timing.refresh_rate_hz < APP_DISPLAY_REFRESH_RATE_MIN_HZ
        || profile->timing.refresh_rate_hz > APP_DISPLAY_REFRESH_RATE_MAX_HZ) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_TIMING, UINT8_MAX};
        return last_issue.status;
    }
    nixie_frame_t frame;
    frame.brightness_pm = profile->brightness_pm;
    memcpy(frame.anode_brightness_pm, profile->anode_brightness_pm, sizeof(frame.anode_brightness_pm));
    memcpy(frame.symbols, symbols, profile->board.element_count);
    nixie_status_t result =
        nixie_compile(&profile->board, &profile->timing, &frame, bsp_nixie_caps(), &scratch_plan, &last_issue);
    if (result == NIXIE_OK) {
        result = bsp_nixie_present(&scratch_plan);
        last_issue.status = result;
    }
    if (result == NIXIE_OK) {
        selected = *profile;
        memcpy(current_symbols, frame.symbols, profile->board.element_count);
        initialized = true;
    }
    return result;
}

bool app_display_init(void) {
    if (initialized) {
        return true;
    }
    if (!bsp_nixie_init()) {
        last_issue = (nixie_issue_t){NIXIE_NOT_READY, UINT8_MAX};
        return false;
    }
    return app_display_configure(&display_default_profile) == NIXIE_OK;
}

nixie_status_t app_display_configure(const app_display_profile_t* profile) {
    uint8_t symbols[NIXIE_MAX_ELEMENTS];
    memset(symbols, NIXIE_SYMBOL_BLANK, sizeof(symbols));
    return publish_configuration(profile, symbols);
}

nixie_status_t app_display_present(const uint8_t* symbols, uint8_t count) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    if (count != selected.board.element_count) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX};
        return last_issue.status;
    }
    return publish_configuration(&selected, symbols);
}

nixie_status_t app_display_set_parameters(uint16_t brightness_pm, uint16_t refresh_rate_hz, uint16_t blanking_time_us) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    app_display_profile_t candidate = selected;
    candidate.brightness_pm = brightness_pm;
    candidate.timing.refresh_rate_hz = refresh_rate_hz;
    candidate.timing.blanking_time_us = blanking_time_us;
    return publish_configuration(&candidate, current_symbols);
}

bool app_display_get_status(app_display_status_t* status) {
    if (status == NULL || !initialized) {
        return false;
    }
    bsp_nixie_status_t hardware;
    if (!bsp_nixie_get_status(&hardware)) {
        return false;
    }
    *status = (app_display_status_t){
        .brightness_pm = selected.brightness_pm,
        .refresh_rate_hz = selected.timing.refresh_rate_hz,
        .blanking_time_us = selected.timing.blanking_time_us,
        .missed_slots = hardware.missed_slots,
        .running = hardware.running,
    };
    return true;
}

nixie_issue_t app_display_last_issue(void) { return last_issue; }

void app_display_process(bool power_ready) {
    if (initialized && power_ready != enabled && bsp_nixie_set_enabled(power_ready)) {
        enabled = power_ready;
    }
}
