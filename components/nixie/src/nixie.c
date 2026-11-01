#include "nixie.h"

#include <stddef.h>

static nixie_status_t report(nixie_issue_t* issue, nixie_status_t status, uint8_t element, uint8_t group) {
    if (issue != NULL) {
        *issue = (nixie_issue_t){status, element, group};
    }
    return status;
}

static bool valid_caps(const nixie_caps_t* caps) {
    return caps != NULL && caps->anode_count > 0U && caps->anode_count <= 16U && caps->cathode_count > 0U
           && caps->cathode_count <= NIXIE_MAX_CATHODES && caps->max_slot_us > 0U;
}

void nixie_frame_clear(nixie_frame_t* frame) {
    if (frame == NULL) {
        return;
    }
    frame->brightness_pm = NIXIE_BRIGHTNESS_MAX;
    for (uint8_t e = 0U; e < NIXIE_MAX_ELEMENTS; ++e) {
        for (uint8_t g = 0U; g < NIXIE_MAX_GROUPS; ++g) {
            frame->cells[e][g] = (nixie_cell_t){NIXIE_BRIGHTNESS_MAX, NIXIE_SYMBOL_BLANK};
        }
    }
}

nixie_status_t nixie_validate_board(const nixie_board_t* board, const nixie_caps_t* caps, nixie_issue_t* issue) {
    if (!valid_caps(caps) || board == NULL || board->elements == NULL || board->element_count == 0U
        || board->element_count > NIXIE_MAX_ELEMENTS) {
        return report(issue, NIXIE_INVALID_ARGUMENT, UINT8_MAX, UINT8_MAX);
    }
    uint16_t anodes = 0U;
    uint16_t fixed_anodes = 0U;
    uint16_t anode_cathodes[16] = {0U};
    uint8_t slot_count = 0U;
    for (uint8_t e = 0U; e < board->element_count; ++e) {
        const nixie_element_t* element = &board->elements[e];
        const nixie_tube_t* tube = element->tube;
        if (tube == NULL || tube->cathode_count == 0U || tube->cathode_count > NIXIE_MAX_CATHODES
            || tube->group_count == 0U || tube->group_count > NIXIE_MAX_GROUPS || tube->symbols == NULL
            || tube->symbol_count == 0U || tube->symbol_count > 32U) {
            return report(issue, NIXIE_INVALID_TUBE, e, UINT8_MAX);
        }
        if ((uint32_t)slot_count + tube->group_count > NIXIE_MAX_SLOTS) {
            return report(issue, NIXIE_INVALID_ARGUMENT, e, UINT8_MAX);
        }
        slot_count += tube->group_count;
        if (element->anode_channel >= caps->anode_count || element->cathode_channels == NULL) {
            return report(issue, NIXIE_INVALID_WIRING, e, UINT8_MAX);
        }
        uint16_t channels = 0U;
        bool fixed_return = false;
        for (uint8_t c = 0U; c < tube->cathode_count; ++c) {
            uint8_t channel = element->cathode_channels[c];
            if (channel == NIXIE_CHANNEL_FIXED && tube->cathode_count == 1U && tube->group_count == 1U) {
                fixed_return = true;
                continue;
            }
            if (channel >= caps->cathode_count || (channels & (1U << channel)) != 0U) {
                return report(issue, NIXIE_INVALID_WIRING, e, UINT8_MAX);
            }
            channels |= (uint16_t)(1U << channel);
        }
        uint16_t anode = (uint16_t)(1U << element->anode_channel);
        /* Every selected cathode also reaches the other elements on this anode.
         * Reject aliases and permanent returns which would light unintentionally. */
        if ((fixed_anodes & anode) != 0U || (fixed_return && (anodes & anode) != 0U)
            || (anode_cathodes[element->anode_channel] & channels) != 0U) {
            return report(issue, NIXIE_INVALID_WIRING, e, UINT8_MAX);
        }
        anodes |= anode;
        anode_cathodes[element->anode_channel] |= channels;
        if (fixed_return) {
            fixed_anodes |= anode;
        }
        uint16_t groups = 0U;
        for (uint8_t s = 0U; s < tube->symbol_count; ++s) {
            const nixie_symbol_t* symbol = &tube->symbols[s];
            if (symbol->id == NIXIE_SYMBOL_BLANK || symbol->group >= tube->group_count || symbol->cathodes == 0U
                || ((uint32_t)symbol->cathodes >> tube->cathode_count) != 0U) {
                return report(issue, NIXIE_INVALID_TUBE, e, symbol->group);
            }
            /* Each slot lights one electrode. Multiple cathodes require an
             * explicitly designed current-sharing circuit and are not inferred. */
            if ((symbol->cathodes & (symbol->cathodes - 1U)) != 0U) {
                return report(issue, NIXIE_INVALID_TUBE, e, symbol->group);
            }
            for (uint8_t other = 0U; other < s; ++other) {
                if (tube->symbols[other].group == symbol->group && tube->symbols[other].id == symbol->id) {
                    return report(issue, NIXIE_INVALID_TUBE, e, symbol->group);
                }
            }
            groups |= (uint16_t)(1U << symbol->group);
        }
        if (groups != (1U << tube->group_count) - 1U) {
            return report(issue, NIXIE_INVALID_TUBE, e, UINT8_MAX);
        }
    }
    return report(issue, NIXIE_OK, UINT8_MAX, UINT8_MAX);
}

nixie_status_t nixie_validate_plan(const nixie_plan_t* plan, const nixie_caps_t* caps) {
    if (!valid_caps(caps) || plan == NULL || plan->slot_count == 0U || plan->slot_count > NIXIE_MAX_SLOTS) {
        return NIXIE_INVALID_ARGUMENT;
    }
    if (plan->timing.refresh_hz == 0U || plan->slot_us > caps->max_slot_us
        || plan->slot_us <= plan->timing.dead_time_us) {
        return NIXIE_INVALID_TIMING;
    }
    uint32_t max_on = plan->slot_us - plan->timing.dead_time_us;
    for (uint8_t s = 0U; s < plan->slot_count; ++s) {
        const nixie_slot_t* slot = &plan->slots[s];
        if (slot->anode >= caps->anode_count || ((uint32_t)slot->cathodes >> caps->cathode_count) != 0U
            || (slot->cathodes & (slot->cathodes - 1U)) != 0U) {
            return NIXIE_INVALID_WIRING;
        }
        if (slot->on_us > max_on) {
            return NIXIE_INVALID_TIMING;
        }
    }
    return NIXIE_OK;
}

nixie_status_t nixie_compile(const nixie_board_t* board, const nixie_timing_t* timing, const nixie_frame_t* frame,
                             const nixie_caps_t* caps, nixie_plan_t* plan, nixie_issue_t* issue) {
    if (timing == NULL || frame == NULL || plan == NULL || frame->brightness_pm > NIXIE_BRIGHTNESS_MAX) {
        return report(issue, NIXIE_INVALID_ARGUMENT, UINT8_MAX, UINT8_MAX);
    }
    nixie_status_t result = nixie_validate_board(board, caps, issue);
    if (result != NIXIE_OK) {
        return result;
    }
    *plan = (nixie_plan_t){.timing = *timing};
    for (uint8_t e = 0U; e < board->element_count; ++e) {
        plan->slot_count += board->elements[e].tube->group_count;
    }
    uint32_t rate = (uint32_t)timing->refresh_hz * plan->slot_count;
    if (rate == 0U) {
        return report(issue, NIXIE_INVALID_TIMING, UINT8_MAX, UINT8_MAX);
    }
    uint32_t slot_us = (1000000UL + rate / 2U) / rate;
    if (slot_us == 0U || slot_us > caps->max_slot_us) {
        return report(issue, NIXIE_INVALID_TIMING, UINT8_MAX, UINT8_MAX);
    }
    plan->slot_us = (uint16_t)slot_us;
    result = nixie_validate_plan(plan, caps);
    if (result != NIXIE_OK) {
        return report(issue, result, UINT8_MAX, UINT8_MAX);
    }
    uint32_t budget = slot_us - timing->dead_time_us;
    uint8_t slot_index = 0U;
    for (uint8_t e = 0U; e < board->element_count; ++e) {
        const nixie_element_t* element = &board->elements[e];
        const nixie_tube_t* tube = element->tube;
        for (uint8_t g = 0U; g < tube->group_count; ++g) {
            const nixie_cell_t* cell = &frame->cells[e][g];
            nixie_slot_t* slot = &plan->slots[slot_index++];
            slot->anode = element->anode_channel;
            if (cell->brightness_pm > NIXIE_BRIGHTNESS_MAX) {
                return report(issue, NIXIE_INVALID_ARGUMENT, e, g);
            }
            if (cell->symbol == NIXIE_SYMBOL_BLANK) {
                continue;
            }
            const nixie_symbol_t* selected = NULL;
            for (uint8_t s = 0U; s < tube->symbol_count; ++s) {
                if (tube->symbols[s].group == g && tube->symbols[s].id == cell->symbol) {
                    selected = &tube->symbols[s];
                    break;
                }
            }
            if (selected == NULL) {
                return report(issue, NIXIE_INVALID_SYMBOL, e, g);
            }
            for (uint8_t c = 0U; c < tube->cathode_count; ++c) {
                uint8_t channel = element->cathode_channels[c];
                if ((selected->cathodes & (1U << c)) != 0U && channel != NIXIE_CHANNEL_FIXED) {
                    slot->cathodes |= (uint16_t)(1U << channel);
                }
            }
            uint32_t brightness = ((uint32_t)frame->brightness_pm * cell->brightness_pm + 500U) / 1000U;
            if (brightness == 0U) {
                continue;
            }
            /* Quantize to microseconds. A pulse rounding to zero is simply blank. */
            slot->on_us = (uint16_t)((budget * brightness + 500U) / 1000U);
        }
    }
    return report(issue, nixie_validate_plan(plan, caps), UINT8_MAX, UINT8_MAX);
}

const char* nixie_status_name(nixie_status_t status) {
    switch (status) {
        case NIXIE_OK: return "OK";
        case NIXIE_INVALID_ARGUMENT: return "invalid argument";
        case NIXIE_INVALID_TUBE: return "invalid tube description";
        case NIXIE_INVALID_WIRING: return "invalid/unassigned wiring";
        case NIXIE_INVALID_SYMBOL: return "unsupported symbol/group";
        case NIXIE_INVALID_TIMING: return "invalid refresh period/dead time";
        case NIXIE_BUSY: return "pending frame busy";
        case NIXIE_NOT_READY: return "backend not ready";
        default: return "unknown error";
    }
}
