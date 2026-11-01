#include "app_display.h"

#include "bsp_nixie.h"
#include "bsp_time.h"
#include "display_profile.h"

#include <stddef.h>
#include <stdio.h>

static app_display_profile_t selected;
static nixie_frame_t frame;
static nixie_plan_t scratch_plan;
static nixie_issue_t last_issue = {NIXIE_NOT_READY, UINT8_MAX, UINT8_MAX};
static uint32_t last_step_ms;
static uint8_t demo_digit;
static bool initialized;
static bool enabled;
static bool dirty;

/* Alternative symbols in group 0 render the current digit. A one-symbol group
 * (an internal point or external separator) renders its sole symbol throughout. */
static void demo_frame(const app_display_profile_t* profile, uint8_t digit) {
    nixie_frame_clear(&frame);
    frame.brightness_pm = profile->brightness_pm;
    for (uint8_t e = 0U; e < profile->board.element_count; ++e) {
        const nixie_tube_t* tube = profile->board.elements[e].tube;
        for (uint8_t g = 0U; g < tube->group_count; ++g) {
            uint8_t chosen = NIXIE_SYMBOL_BLANK;
            for (uint8_t s = 0U; s < tube->symbol_count; ++s) {
                if (tube->symbols[s].group != g) {
                    continue;
                }
                if (chosen == NIXIE_SYMBOL_BLANK || tube->symbols[s].id == digit) {
                    chosen = tube->symbols[s].id;
                }
                if (tube->symbols[s].id == digit) {
                    break;
                }
            }
            frame.cells[e][g].symbol = chosen;
            frame.cells[e][g].brightness_pm = profile->cell_brightness_pm[e][g];
        }
    }
}

static nixie_status_t publish_configuration(const app_display_profile_t* profile, bool restart_demo) {
    if (profile == NULL || profile->demo_step_ms == 0U) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX, UINT8_MAX};
        return last_issue.status;
    }
    nixie_status_t result = nixie_validate_board(&profile->board, bsp_nixie_caps(), &last_issue);
    if (result != NIXIE_OK) {
        return result;
    }
    /* Reject a profile which would work for 0 but fail later in the demo. */
    for (uint8_t digit = 0U; digit < 10U; ++digit) {
        demo_frame(profile, digit);
        result = nixie_compile(&profile->board, &profile->timing, &frame, bsp_nixie_caps(), &scratch_plan, &last_issue);
        if (result != NIXIE_OK) {
            return result;
        }
    }
    uint8_t next_digit = restart_demo ? 0U : demo_digit;
    demo_frame(profile, next_digit);
    result = nixie_compile(&profile->board, &profile->timing, &frame, bsp_nixie_caps(), &scratch_plan, &last_issue);
    if (result == NIXIE_OK) {
        result = bsp_nixie_present(&scratch_plan);
        last_issue.status = result;
    }
    if (result == NIXIE_OK) {
        selected = *profile;
        demo_digit = next_digit;
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
        last_issue = (nixie_issue_t){NIXIE_NOT_READY, UINT8_MAX, UINT8_MAX};
        return false;
    }
    return app_display_configure(&display_default_profile) == NIXIE_OK;
}

nixie_status_t app_display_configure(const app_display_profile_t* profile) {
    return publish_configuration(profile, true);
}

nixie_status_t app_display_set_brightness(uint16_t brightness_pm) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    app_display_profile_t candidate = selected;
    candidate.brightness_pm = brightness_pm;
    return publish_configuration(&candidate, false);
}

nixie_status_t app_display_set_timing(const nixie_timing_t* timing) {
    if (!initialized || timing == NULL) {
        return timing == NULL ? NIXIE_INVALID_ARGUMENT : NIXIE_NOT_READY;
    }
    app_display_profile_t candidate = selected;
    candidate.timing = *timing;
    return publish_configuration(&candidate, false);
}

nixie_status_t app_display_set_cell_brightness(uint8_t element, uint8_t group, uint16_t brightness_pm) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    if (element >= selected.board.element_count || group >= selected.board.elements[element].tube->group_count) {
        return NIXIE_INVALID_ARGUMENT;
    }
    app_display_profile_t candidate = selected;
    candidate.cell_brightness_pm[element][group] = brightness_pm;
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
        /* Hold the first/current digit while HV is unavailable; show it for a
         * complete demo interval when illumination is first authorized. */
        last_step_ms = now;
    }
    if ((uint32_t)(now - last_step_ms) >= selected.demo_step_ms) {
        demo_digit = (uint8_t)((demo_digit + 1U) % 10U);
        last_step_ms = now;
        dirty = true;
    }
    if (dirty) {
        demo_frame(&selected, demo_digit);
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
            printf("Display stopped: %s, element=%u group=%u\n", nixie_status_name(result),
                   (unsigned)last_issue.element, (unsigned)last_issue.group);
            return;
        }
    }
    if (power_ready && !enabled) {
        enabled = bsp_nixie_set_enabled(true);
    }
}
