#include "nixie.h"

#include <stddef.h>

static nixie_status_t report(nixie_issue_t* issue, nixie_status_t status, uint8_t element) {
    if (issue != NULL) {
        *issue = (nixie_issue_t){status, element};
    }
    return status;
}

static bool valid_caps(const nixie_caps_t* caps) {
    return caps != NULL && caps->anode_count > 0U && caps->anode_count <= NIXIE_MAX_ANODES && caps->cathode_count > 0U
           && caps->cathode_count <= NIXIE_MAX_CATHODES && caps->max_slot_us > 0U;
}

nixie_status_t nixie_validate_board(const nixie_board_t* board, const nixie_caps_t* caps, nixie_issue_t* issue) {
    if (!valid_caps(caps) || board == NULL || board->elements == NULL || board->element_count == 0U
        || board->element_count > NIXIE_MAX_ELEMENTS) {
        return report(issue, NIXIE_INVALID_ARGUMENT, UINT8_MAX);
    }
    uint16_t anodes = 0U;
    uint16_t fixed_anodes = 0U;
    uint16_t anode_cathodes[NIXIE_MAX_ANODES] = {0U};
    for (uint8_t e = 0U; e < board->element_count; ++e) {
        const nixie_element_t* element = &board->elements[e];
        const nixie_tube_t* tube = element->tube;
        if (tube == NULL || tube->cathode_count == 0U || tube->cathode_count > NIXIE_MAX_CATHODES
            || tube->symbols == NULL || tube->symbol_count == 0U || tube->symbol_count > 32U) {
            return report(issue, NIXIE_INVALID_TUBE, e);
        }
        if (element->anode_channel >= caps->anode_count || element->cathode_channels == NULL) {
            return report(issue, NIXIE_INVALID_WIRING, e);
        }
        uint16_t channels = 0U;
        bool fixed_return = false;
        for (uint8_t c = 0U; c < tube->cathode_count; ++c) {
            uint8_t channel = element->cathode_channels[c];
            if (channel == NIXIE_CHANNEL_FIXED && tube->cathode_count == 1U) {
                fixed_return = true;
                continue;
            }
            if (channel >= caps->cathode_count || (channels & (1U << channel)) != 0U) {
                return report(issue, NIXIE_INVALID_WIRING, e);
            }
            channels |= (uint16_t)(1U << channel);
        }
        uint16_t anode = (uint16_t)(1U << element->anode_channel);
        /* Shared-anode elements are selected simultaneously. Reject cathode
         * aliases and fixed returns which would light an unselected element. */
        if ((fixed_anodes & anode) != 0U || (fixed_return && (anodes & anode) != 0U)
            || (anode_cathodes[element->anode_channel] & channels) != 0U) {
            return report(issue, NIXIE_INVALID_WIRING, e);
        }
        anodes |= anode;
        anode_cathodes[element->anode_channel] |= channels;
        if (fixed_return) {
            fixed_anodes |= anode;
        }
        for (uint8_t s = 0U; s < tube->symbol_count; ++s) {
            const nixie_symbol_t* symbol = &tube->symbols[s];
            if (symbol->id == NIXIE_SYMBOL_BLANK || symbol->cathode >= tube->cathode_count) {
                return report(issue, NIXIE_INVALID_TUBE, e);
            }
            for (uint8_t other = 0U; other < s; ++other) {
                if (tube->symbols[other].id == symbol->id) {
                    return report(issue, NIXIE_INVALID_TUBE, e);
                }
            }
        }
    }
    return report(issue, NIXIE_OK, UINT8_MAX);
}

nixie_status_t nixie_validate_plan(const nixie_plan_t* plan, const nixie_caps_t* caps) {
    if (!valid_caps(caps) || plan == NULL || plan->slot_count == 0U || plan->slot_count > caps->anode_count) {
        return NIXIE_INVALID_ARGUMENT;
    }
    if (plan->timing.refresh_rate_hz == 0U || plan->slot_us > caps->max_slot_us
        || plan->slot_us <= plan->timing.blanking_time_us) {
        return NIXIE_INVALID_TIMING;
    }
    uint32_t max_on = plan->slot_us - plan->timing.blanking_time_us;
    uint16_t anodes = 0U;
    for (uint8_t s = 0U; s < plan->slot_count; ++s) {
        const nixie_slot_t* slot = &plan->slots[s];
        if (slot->anode >= caps->anode_count || (anodes & (1U << slot->anode)) != 0U
            || ((uint32_t)slot->cathodes >> caps->cathode_count) != 0U) {
            return NIXIE_INVALID_WIRING;
        }
        anodes |= (uint16_t)(1U << slot->anode);
        if (slot->on_us > max_on) {
            return NIXIE_INVALID_TIMING;
        }
    }
    return NIXIE_OK;
}

nixie_status_t nixie_compile(const nixie_board_t* board, const nixie_timing_t* timing, const nixie_frame_t* frame,
                             const nixie_caps_t* caps, nixie_plan_t* plan, nixie_issue_t* issue) {
    if (timing == NULL || frame == NULL || plan == NULL || frame->brightness_pm > NIXIE_BRIGHTNESS_MAX) {
        return report(issue, NIXIE_INVALID_ARGUMENT, UINT8_MAX);
    }
    nixie_status_t result = nixie_validate_board(board, caps, issue);
    if (result != NIXIE_OK) {
        return result;
    }
    plan->timing = *timing;
    plan->slot_count = 0U;
    uint8_t anode_slots[NIXIE_MAX_ANODES] = {0U};
    uint16_t anodes = 0U;
    uint16_t lit_anodes = 0U;
    /* Reserve anodes and combine their selected cathodes in one board pass.
     * Dark elements still reserve the slot, preserving the scan period. */
    for (uint8_t e = 0U; e < board->element_count; ++e) {
        const nixie_element_t* element = &board->elements[e];
        uint8_t anode = element->anode_channel;
        uint16_t bit = (uint16_t)(1U << anode);
        if ((anodes & bit) == 0U) {
            if (frame->anode_brightness_pm[anode] > NIXIE_BRIGHTNESS_MAX) {
                return report(issue, NIXIE_INVALID_ARGUMENT, e);
            }
            anode_slots[anode] = plan->slot_count;
            plan->slots[plan->slot_count++] = (nixie_slot_t){.anode = anode};
            anodes |= bit;
        }
        if (frame->symbols[e] == NIXIE_SYMBOL_BLANK) {
            continue;
        }
        const nixie_tube_t* tube = element->tube;
        const nixie_symbol_t* selected = NULL;
        for (uint8_t s = 0U; s < tube->symbol_count; ++s) {
            if (tube->symbols[s].id == frame->symbols[e]) {
                selected = &tube->symbols[s];
                break;
            }
        }
        if (selected == NULL) {
            return report(issue, NIXIE_INVALID_SYMBOL, e);
        }
        uint8_t channel = element->cathode_channels[selected->cathode];
        if (channel != NIXIE_CHANNEL_FIXED) {
            plan->slots[anode_slots[anode]].cathodes |= (uint16_t)(1U << channel);
        }
        lit_anodes |= bit;
    }
    uint32_t rate = (uint32_t)timing->refresh_rate_hz * plan->slot_count;
    if (rate == 0U) {
        return report(issue, NIXIE_INVALID_TIMING, UINT8_MAX);
    }
    uint32_t slot_us = (1000000UL + rate / 2U) / rate;
    if (slot_us > caps->max_slot_us || slot_us <= timing->blanking_time_us) {
        return report(issue, NIXIE_INVALID_TIMING, UINT8_MAX);
    }
    plan->slot_us = (uint16_t)slot_us;
    uint32_t budget = slot_us - timing->blanking_time_us;
    for (uint8_t s = 0U; s < plan->slot_count; ++s) {
        nixie_slot_t* slot = &plan->slots[s];
        if ((lit_anodes & (1U << slot->anode)) != 0U) {
            uint32_t brightness =
                ((uint32_t)frame->brightness_pm * frame->anode_brightness_pm[slot->anode] + 500U) / 1000U;
            /* All selected cathodes share the anode PWM. A pulse quantized to
             * zero stays dark without removing the slot from the scan. */
            slot->on_us = (uint16_t)((budget * brightness + 500U) / 1000U);
        }
    }
    return report(issue, NIXIE_OK, UINT8_MAX);
}

const char* nixie_status_name(nixie_status_t status) {
    switch (status) {
        case NIXIE_OK: return "OK";
        case NIXIE_INVALID_ARGUMENT: return "invalid argument";
        case NIXIE_INVALID_TUBE: return "invalid tube description";
        case NIXIE_INVALID_WIRING: return "invalid/unassigned wiring";
        case NIXIE_INVALID_SYMBOL: return "unsupported symbol";
        case NIXIE_INVALID_TIMING: return "invalid refresh rate/blanking time";
        case NIXIE_BUSY: return "pending frame busy";
        case NIXIE_NOT_READY: return "backend not ready";
        default: return "unknown error";
    }
}
