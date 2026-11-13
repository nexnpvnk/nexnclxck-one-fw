#include "app_display.h"

#include "app_clock.h"
#include "bsp_nixie.h"
#include "bsp_time.h"
#include "display_profile.h"

#include <stddef.h>
#include <string.h>

static app_display_profile_t selected;
static uint8_t current_symbols[NIXIE_MAX_ELEMENTS];
static nixie_plan_t scratch_plan;
static nixie_issue_t last_issue = {NIXIE_NOT_READY, UINT8_MAX};
static bool initialized;
static bool enabled;
static uint32_t last_clock_poll_ms;

static nixie_status_t validate_clock_roles(const app_display_profile_t* profile) {
    if (profile->clock_roles == NULL) {
        return NIXIE_OK;
    }
    static const uint16_t required_symbols[] = {
        [APP_DISPLAY_CLOCK_UNUSED] = 0U,           /* Always blank. */
        [APP_DISPLAY_CLOCK_HOUR_TENS] = 0x007U,    /* Digits 0..2. */
        [APP_DISPLAY_CLOCK_HOUR_UNITS] = 0x3FFU,   /* Digits 0..9. */
        [APP_DISPLAY_CLOCK_MINUTE_TENS] = 0x03FU,  /* Digits 0..5. */
        [APP_DISPLAY_CLOCK_MINUTE_UNITS] = 0x3FFU, /* Digits 0..9. */
        [APP_DISPLAY_CLOCK_SECOND_TENS] = 0x03FU,  /* Digits 0..5. */
        [APP_DISPLAY_CLOCK_SECOND_UNITS] = 0x3FFU, /* Digits 0..9. */
        [APP_DISPLAY_CLOCK_SEPARATOR] = 1U << NIXIE_SYMBOL_ON,
    };
    uint16_t separator_anodes = 0U;
    uint16_t other_anodes = 0U;
    for (uint8_t e = 0U; e < profile->board.element_count; ++e) {
        app_display_clock_role_t role = profile->clock_roles[e];
        if ((unsigned)role > APP_DISPLAY_CLOCK_SEPARATOR) {
            last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, e};
            return last_issue.status;
        }
        uint16_t anode = (uint16_t)(1U << profile->board.elements[e].anode_channel);
        if (role == APP_DISPLAY_CLOCK_SEPARATOR) {
            separator_anodes |= anode;
        } else if (role != APP_DISPLAY_CLOCK_UNUSED) {
            other_anodes |= anode;
        }
        if ((separator_anodes & other_anodes) != 0U) {
            last_issue = (nixie_issue_t){NIXIE_INVALID_WIRING, e};
            return last_issue.status;
        }
        const nixie_tube_t* tube = profile->board.elements[e].tube;
        uint16_t available = 0U;
        for (uint8_t s = 0U; s < tube->symbol_count; ++s) {
            if (tube->symbols[s].id < 16U) {
                available |= (uint16_t)(1U << tube->symbols[s].id);
            }
        }
        if ((available & required_symbols[role]) != required_symbols[role]) {
            last_issue = (nixie_issue_t){NIXIE_INVALID_SYMBOL, e};
            return last_issue.status;
        }
    }
    return NIXIE_OK;
}

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
        uint16_t rtc_anodes = 0U;
        if (profile->clock_roles != NULL) {
            for (uint8_t e = 0U; e < profile->board.element_count; ++e) {
                if (profile->clock_roles[e] == APP_DISPLAY_CLOCK_SEPARATOR) {
                    rtc_anodes |= (uint16_t)(1U << profile->board.elements[e].anode_channel);
                }
            }
        }
        result = bsp_nixie_present(&scratch_plan, rtc_anodes);
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
    if (profile == NULL) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX};
        return last_issue.status;
    }
    /* Validate the borrowed layout and its complete symbol set once, before
     * selecting it. Ordinary frame updates retain the validated profile. */
    nixie_status_t result = nixie_validate_board(&profile->board, bsp_nixie_caps(), &last_issue);
    if (result != NIXIE_OK || (result = validate_clock_roles(profile)) != NIXIE_OK) {
        return result;
    }
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
        .running = hardware.running,
        .separators_on = hardware.separators_on,
    };
    return true;
}

nixie_issue_t app_display_last_issue(void) { return last_issue; }

static void present_clock(void) {
    if (selected.clock_roles == NULL) {
        return;
    }
    uint32_t now = bsp_time_ms();
    if (now == last_clock_poll_ms) {
        return;
    }
    last_clock_poll_ms = now;
    app_clock_status_t status;
    if (!app_clock_get_status(&status)) {
        return;
    }
    bool time_valid = status.rtc_ready && status.source_ready;
    app_clock_datetime_t datetime;
    if (time_valid) {
        if (!app_clock_get(&datetime)) {
            return; /* A transient snapshot failure keeps the accepted state. */
        }
        time_valid = datetime.hour <= 23U && datetime.minute <= 59U && datetime.second <= 59U;
    }
    uint8_t symbols[NIXIE_MAX_ELEMENTS];
    memset(symbols, NIXIE_SYMBOL_BLANK, selected.board.element_count);
    if (time_valid) {
        const uint8_t values[] = {
            [APP_DISPLAY_CLOCK_UNUSED] = NIXIE_SYMBOL_BLANK,
            [APP_DISPLAY_CLOCK_HOUR_TENS] = datetime.hour / 10U,
            [APP_DISPLAY_CLOCK_HOUR_UNITS] = datetime.hour % 10U,
            [APP_DISPLAY_CLOCK_MINUTE_TENS] = datetime.minute / 10U,
            [APP_DISPLAY_CLOCK_MINUTE_UNITS] = datetime.minute % 10U,
            [APP_DISPLAY_CLOCK_SECOND_TENS] = datetime.second / 10U,
            [APP_DISPLAY_CLOCK_SECOND_UNITS] = datetime.second % 10U,
            [APP_DISPLAY_CLOCK_SEPARATOR] = NIXIE_SYMBOL_ON,
        };
        for (uint8_t e = 0U; e < selected.board.element_count; ++e) {
            symbols[e] = values[selected.clock_roles[e]];
        }
    }
    /* Accepted symbols are the only cache. BUSY leaves them unchanged, so the
     * next foreground poll retries the latest time rather than dropping it. */
    if (memcmp(symbols, current_symbols, selected.board.element_count) != 0) {
        (void)app_display_present(symbols, selected.board.element_count);
    }
}

void app_display_process(bool power_ready) {
    if (!initialized) {
        return;
    }
    if (power_ready != enabled && bsp_nixie_set_enabled(power_ready)) {
        enabled = power_ready;
    }
    present_clock();
}
