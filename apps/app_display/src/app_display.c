#include "app_display.h"

#include "bsp_nixie.h"
#include "bsp_time.h"
#include "display_profile.h"

#include <stddef.h>
#include <stdio.h>

static app_display_profile_t selected;
static nixie_frame_t frame;
static nixie_plan_t scratch_plan;
static nixie_issue_t last_issue = {NIXIE_NOT_READY, UINT8_MAX};
static uint32_t last_step_ms;
static uint32_t demo_step;
static uint8_t demo_point_fill;
static bool initialized;
static bool enabled;
static bool dirty;

/* Numeric elements cycle through all their symbols in descriptor order, with a
 * one-symbol offset between numeric elements. Other elements keep their first
 * symbol, except the configured points used by the fill pattern. */
static void demo_frame(const app_display_profile_t* profile, uint32_t step, uint8_t point_fill) {
    nixie_frame_clear(&frame);
    frame.brightness_pm = profile->brightness_pm;
    for (uint8_t a = 0U; a < NIXIE_MAX_ANODES; ++a) {
        frame.anode_brightness_pm[a] = profile->anode_brightness_pm[a];
    }
    uint8_t symbol_offset = 0U;
    for (uint8_t e = 0U; e < profile->board.element_count; ++e) {
        const nixie_tube_t* tube = profile->board.elements[e].tube;
        bool has_digits = false;
        for (uint8_t s = 0U; s < tube->symbol_count; ++s) {
            if (tube->symbols[s].id < 10U) {
                has_digits = true;
                break;
            }
        }
        uint8_t index = has_digits ? (uint8_t)((step % tube->symbol_count + symbol_offset) % tube->symbol_count) : 0U;
        frame.symbols[e] = tube->symbols[index].id;
        if (has_digits) {
            ++symbol_offset;
        }
    }
    for (uint8_t p = point_fill; p < profile->demo_point_count; ++p) {
        frame.symbols[profile->demo_point_order[p]] = NIXIE_SYMBOL_BLANK;
    }
}

static nixie_status_t publish_configuration(const app_display_profile_t* profile, bool restart_demo) {
    if (profile == NULL || profile->demo_step_ms == 0U) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX};
        return last_issue.status;
    }
    if (profile->timing.refresh_rate_hz < APP_DISPLAY_REFRESH_RATE_MIN_HZ
        || profile->timing.refresh_rate_hz > APP_DISPLAY_REFRESH_RATE_MAX_HZ) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_TIMING, UINT8_MAX};
        return last_issue.status;
    }
    nixie_status_t result = nixie_validate_board(&profile->board, bsp_nixie_caps(), &last_issue);
    if (result != NIXIE_OK) {
        return result;
    }
    if (profile->demo_point_count > profile->board.element_count
        || (profile->demo_point_count != 0U && profile->demo_point_order == NULL)) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX};
        return last_issue.status;
    }
    uint32_t points = 0U;
    for (uint8_t p = 0U; p < profile->demo_point_count; ++p) {
        uint8_t e = profile->demo_point_order[p];
        if (e >= profile->board.element_count || (points & (1UL << e)) != 0U) {
            last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, e};
            return last_issue.status;
        }
        points |= 1UL << e;
    }
    /* Cover every demo symbol with all points visible. Partially filled patterns
     * only remove cathodes from the same anode slot and cannot add a symbol. */
    uint8_t validation_steps = 1U;
    for (uint8_t e = 0U; e < profile->board.element_count; ++e) {
        uint8_t count = profile->board.elements[e].tube->symbol_count;
        if (count > validation_steps) {
            validation_steps = count;
        }
    }
    for (uint8_t step = 0U; step < validation_steps; ++step) {
        demo_frame(profile, step, profile->demo_point_count);
        result = nixie_compile(&profile->board, &profile->timing, &frame, bsp_nixie_caps(), &scratch_plan, &last_issue);
        if (result != NIXIE_OK) {
            return result;
        }
    }
    uint32_t next_step = restart_demo ? 0U : demo_step;
    uint8_t next_fill = restart_demo ? 0U : demo_point_fill;
    demo_frame(profile, next_step, next_fill);
    result = nixie_compile(&profile->board, &profile->timing, &frame, bsp_nixie_caps(), &scratch_plan, &last_issue);
    if (result == NIXIE_OK) {
        result = bsp_nixie_present(&scratch_plan);
        last_issue.status = result;
    }
    if (result == NIXIE_OK) {
        selected = *profile;
        demo_step = next_step;
        demo_point_fill = next_fill;
        dirty = false;
        if (restart_demo) {
            last_step_ms = bsp_time_ms();
        }
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
    return publish_configuration(profile, true);
}

nixie_status_t app_display_set_parameters(uint16_t brightness_pm, uint16_t refresh_rate_hz, uint16_t blanking_time_us) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    app_display_profile_t candidate = selected;
    candidate.brightness_pm = brightness_pm;
    candidate.timing.refresh_rate_hz = refresh_rate_hz;
    candidate.timing.blanking_time_us = blanking_time_us;
    return publish_configuration(&candidate, false);
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

nixie_status_t app_display_set_brightness(uint16_t brightness_pm) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    return app_display_set_parameters(brightness_pm, selected.timing.refresh_rate_hz, selected.timing.blanking_time_us);
}

nixie_status_t app_display_set_timing(const nixie_timing_t* timing) {
    if (!initialized || timing == NULL) {
        return timing == NULL ? NIXIE_INVALID_ARGUMENT : NIXIE_NOT_READY;
    }
    app_display_profile_t candidate = selected;
    candidate.timing = *timing;
    return publish_configuration(&candidate, false);
}

nixie_status_t app_display_set_anode_brightness(uint8_t anode, uint16_t brightness_pm) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    if (anode >= bsp_nixie_caps()->anode_count || brightness_pm > NIXIE_BRIGHTNESS_MAX) {
        return NIXIE_INVALID_ARGUMENT;
    }
    app_display_profile_t candidate = selected;
    candidate.anode_brightness_pm[anode] = brightness_pm;
    return publish_configuration(&candidate, false);
}

nixie_issue_t app_display_last_issue(void) { return last_issue; }

void app_display_process(bool power_ready) {
    if (!initialized) {
        return;
    }
    if (!power_ready && enabled) {
        (void)bsp_nixie_set_enabled(false);
        enabled = false;
    }
    uint32_t now = bsp_time_ms();
    if (!enabled) {
        /* Hold the first/current symbol while HV is unavailable; show it for a
         * complete demo interval when illumination is first authorized. */
        last_step_ms = now;
    }
    if ((uint32_t)(now - last_step_ms) >= selected.demo_step_ms) {
        ++demo_step;
        demo_point_fill = demo_point_fill < selected.demo_point_count ? (uint8_t)(demo_point_fill + 1U) : 0U;
        last_step_ms = now;
        dirty = true;
    }
    if (dirty) {
        demo_frame(&selected, demo_step, demo_point_fill);
        nixie_status_t result =
            nixie_compile(&selected.board, &selected.timing, &frame, bsp_nixie_caps(), &scratch_plan, &last_issue);
        if (result == NIXIE_OK) {
            result = bsp_nixie_present(&scratch_plan);
            last_issue.status = result;
        }
        if (result == NIXIE_OK) {
            dirty = false;
        } else if (result != NIXIE_BUSY) {
            (void)bsp_nixie_set_enabled(false);
            enabled = false;
            initialized = false;
            printf("Display stopped: %s, element=%u\n", nixie_status_name(result), (unsigned)last_issue.element);
            return;
        }
    }
    if (power_ready && !enabled) {
        enabled = bsp_nixie_set_enabled(true);
    }
}
