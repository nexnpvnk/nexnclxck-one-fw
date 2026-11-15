#include "app_display.h"

#include "bsp_nixie.h"
#include "display_profile.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    nixie_timing_t timing;
    uint16_t brightness_pm;
} display_parameters_t;

static display_parameters_t parameters;
static app_display_frame_t current_frame;
static nixie_plan_t scratch_plan;
static nixie_issue_t last_issue = {NIXIE_NOT_READY, UINT8_MAX};
static bool initialized;
static bool enabled;

static nixie_status_t publish_configuration(const display_parameters_t* candidate, const app_display_frame_t* content) {
    const app_display_profile_t* profile = &display_default_profile;
    if (content == NULL ||
        profile->board.element_count == 0U ||
        profile->board.element_count > NIXIE_MAX_ELEMENTS ||
        content->element_count != profile->board.element_count ||
        ((uint32_t)content->blink_elements >> profile->board.element_count) != 0U) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX};
        return last_issue.status;
    }
    if (candidate->timing.refresh_rate_hz < APP_DISPLAY_REFRESH_RATE_MIN_HZ ||
        candidate->timing.refresh_rate_hz > APP_DISPLAY_REFRESH_RATE_MAX_HZ) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_TIMING, UINT8_MAX};
        return last_issue.status;
    }
    nixie_frame_t frame;
    frame.brightness_pm = candidate->brightness_pm;
    frame.dot_elements = content->dot_elements;
    memcpy(frame.anode_brightness_pm, profile->anode_brightness_pm, sizeof(frame.anode_brightness_pm));
    memcpy(frame.symbols, content->symbols, content->element_count);
    nixie_status_t result =
        nixie_compile(&profile->board, &candidate->timing, &frame, bsp_nixie_caps(), &scratch_plan, &last_issue);
    if (result == NIXIE_OK) {
        uint16_t rtc_anodes = 0U;
        for (uint8_t e = 0U; e < content->element_count; ++e) {
            if ((content->blink_elements & (1U << e)) != 0U) {
                rtc_anodes |= (uint16_t)(1U << profile->board.elements[e].anode_channel);
            }
        }
        result = bsp_nixie_present(&scratch_plan, rtc_anodes);
        last_issue.status = result;
    }
    if (result == NIXIE_OK) {
        parameters = *candidate;
        current_frame = *content;
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
    const display_parameters_t defaults = {
        .timing = display_default_profile.timing,
        .brightness_pm = display_default_profile.brightness_pm,
    };
    app_display_frame_t blank = {.element_count = display_default_profile.board.element_count};
    memset(blank.symbols, NIXIE_SYMBOL_BLANK, sizeof(blank.symbols));
    return publish_configuration(&defaults, &blank) == NIXIE_OK;
}

const app_display_profile_t* app_display_profile(void) {
    return &display_default_profile;
}

nixie_status_t app_display_present(const app_display_frame_t* frame) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    if (frame == NULL || frame->element_count != display_default_profile.board.element_count) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX};
        return last_issue.status;
    }
    /* Cache only accepted content. BUSY leaves it unchanged so the UI can retry. */
    if (frame->dot_elements == current_frame.dot_elements &&
        frame->blink_elements == current_frame.blink_elements &&
        memcmp(frame->symbols, current_frame.symbols, frame->element_count) == 0) {
        return NIXIE_OK;
    }
    return publish_configuration(&parameters, frame);
}

nixie_status_t app_display_set_parameters(uint16_t brightness_pm, uint16_t refresh_rate_hz, uint16_t blanking_time_us) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    display_parameters_t candidate = parameters;
    candidate.brightness_pm = brightness_pm;
    candidate.timing.refresh_rate_hz = refresh_rate_hz;
    candidate.timing.blanking_time_us = blanking_time_us;
    return publish_configuration(&candidate, &current_frame);
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
        .brightness_pm = parameters.brightness_pm,
        .refresh_rate_hz = parameters.timing.refresh_rate_hz,
        .blanking_time_us = parameters.timing.blanking_time_us,
        .running = hardware.running,
        .separators_on = hardware.separators_on,
    };
    return true;
}

nixie_issue_t app_display_last_issue(void) {
    return last_issue;
}

bool app_display_process(bool power_ready) {
    if (!initialized) {
        return false;
    }
    if (power_ready != enabled && bsp_nixie_set_enabled(power_ready)) {
        enabled = power_ready;
    }
    return enabled;
}
