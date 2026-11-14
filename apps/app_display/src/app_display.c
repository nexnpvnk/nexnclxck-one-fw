#include "app_display.h"

#include "app_bme280.h"
#include "app_clock.h"
#include "bsp_nixie.h"
#include "bsp_time.h"
#include "display_profile.h"

#include <stddef.h>
#include <string.h>

#define APP_DISPLAY_TIME_MS   40000U
#define APP_DISPLAY_INFO_MS   5000U
#define APP_DISPLAY_MANUAL_MS 10000U
#define APP_DISPLAY_POLL_MS   20U

typedef enum {
    SCREEN_TIME = 0,
    SCREEN_DATE,
    SCREEN_TEMPERATURE,
    SCREEN_HUMIDITY,
    SCREEN_PRESSURE,
    SCREEN_COUNT,
} screen_t;

static app_display_profile_t selected;
static uint8_t current_symbols[NIXIE_MAX_ELEMENTS];
static uint16_t current_dots;
static uint16_t current_rtc_anodes;
static nixie_plan_t scratch_plan;
static nixie_issue_t last_issue = {NIXIE_NOT_READY, UINT8_MAX};
static bool initialized;
static bool enabled;
static screen_t screen;
static uint32_t screen_started_ms;
static uint32_t last_poll_ms;
static bool manual;

static uint16_t available_symbols(const nixie_tube_t* tube) {
    uint16_t mask = 0U;
    for (uint8_t s = 0U; s < tube->symbol_count; ++s) {
        if (tube->symbols[s].id < 16U) {
            mask |= (uint16_t)(1U << tube->symbols[s].id);
        }
    }
    return mask;
}

static nixie_status_t validate_layout(const app_display_profile_t* profile) {
    const app_display_layout_t* layout = profile->layout;
    if (layout == NULL) {
        return NIXIE_OK;
    }
    if ((layout->digit_count != 4U && layout->digit_count != 6U)
        || (layout->digit_count == 4U
            && (layout->points[APP_DISPLAY_POINT_RIGHT_TOP] != NIXIE_CHANNEL_UNASSIGNED
                || layout->points[APP_DISPLAY_POINT_RIGHT_BOTTOM] != NIXIE_CHANNEL_UNASSIGNED))) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX};
        return last_issue.status;
    }
    uint16_t used = 0U;
    uint16_t separator_anodes = 0U;
    uint16_t digit_anodes = 0U;
    for (uint8_t i = 0U; i < layout->digit_count + APP_DISPLAY_POINT_COUNT; ++i) {
        bool digit = i < layout->digit_count;
        uint8_t e = digit ? layout->digits[i] : layout->points[i - layout->digit_count];
        if (!digit && e == NIXIE_CHANNEL_UNASSIGNED) {
            continue;
        }
        if (e >= profile->board.element_count || (used & (1U << e)) != 0U) {
            last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, e};
            return last_issue.status;
        }
        used |= (uint16_t)(1U << e);
        uint16_t anode = (uint16_t)(1U << profile->board.elements[e].anode_channel);
        if (digit) {
            digit_anodes |= anode;
        } else {
            separator_anodes |= anode;
        }
        if ((separator_anodes & digit_anodes) != 0U) {
            last_issue = (nixie_issue_t){NIXIE_INVALID_WIRING, e};
            return last_issue.status;
        }
        uint16_t required = digit ? 0x3FFU : (1U << NIXIE_SYMBOL_ON);
        if ((available_symbols(profile->board.elements[e].tube) & required) != required) {
            last_issue = (nixie_issue_t){NIXIE_INVALID_SYMBOL, e};
            return last_issue.status;
        }
    }
    return NIXIE_OK;
}

static nixie_status_t publish_configuration(const app_display_profile_t* profile, const uint8_t* symbols,
                                             uint16_t dots, uint16_t rtc_anodes) {
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
    frame.dot_elements = dots;
    memcpy(frame.anode_brightness_pm, profile->anode_brightness_pm, sizeof(frame.anode_brightness_pm));
    memcpy(frame.symbols, symbols, profile->board.element_count);
    nixie_status_t result =
        nixie_compile(&profile->board, &profile->timing, &frame, bsp_nixie_caps(), &scratch_plan, &last_issue);
    if (result == NIXIE_OK) {
        result = bsp_nixie_present(&scratch_plan, rtc_anodes);
        last_issue.status = result;
    }
    if (result == NIXIE_OK) {
        selected = *profile;
        memcpy(current_symbols, frame.symbols, profile->board.element_count);
        current_dots = dots;
        current_rtc_anodes = rtc_anodes;
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
    if (result != NIXIE_OK || (result = validate_layout(profile)) != NIXIE_OK) {
        return result;
    }
    uint8_t symbols[NIXIE_MAX_ELEMENTS];
    memset(symbols, NIXIE_SYMBOL_BLANK, sizeof(symbols));
    result = publish_configuration(profile, symbols, 0U, 0U);
    if (result == NIXIE_OK) {
        screen = SCREEN_TIME;
        manual = false;
        screen_started_ms = bsp_time_ms();
        last_poll_ms = screen_started_ms - APP_DISPLAY_POLL_MS;
    }
    return result;
}

nixie_status_t app_display_present(const uint8_t* symbols, uint8_t count) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    if (count != selected.board.element_count) {
        last_issue = (nixie_issue_t){NIXIE_INVALID_ARGUMENT, UINT8_MAX};
        return last_issue.status;
    }
    return publish_configuration(&selected, symbols, 0U, 0U);
}

nixie_status_t app_display_set_parameters(uint16_t brightness_pm, uint16_t refresh_rate_hz, uint16_t blanking_time_us) {
    if (!initialized) {
        return NIXIE_NOT_READY;
    }
    app_display_profile_t candidate = selected;
    candidate.brightness_pm = brightness_pm;
    candidate.timing.refresh_rate_hz = refresh_rate_hz;
    candidate.timing.blanking_time_us = blanking_time_us;
    return publish_configuration(&candidate, current_symbols, current_dots, current_rtc_anodes);
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

/* A failed snapshot is transient. Readiness/range failures are explicit and
 * may blank time, without preventing independent sensor screens. */
static bool read_clock(app_clock_datetime_t* datetime, bool* time_valid) {
    app_clock_status_t status;
    if (!app_clock_get_status(&status)) {
        return false;
    }
    *time_valid = status.rtc_ready && status.source_ready;
    if (*time_valid) {
        if (!app_clock_get(datetime)) {
            return false;
        }
        *time_valid = datetime->hour <= 23U && datetime->minute <= 59U && datetime->second <= 59U;
    }
    return true;
}

static uint8_t numeric_top_point(void) {
    return selected.layout->digit_count == 6U ? APP_DISPLAY_POINT_RIGHT_TOP : APP_DISPLAY_POINT_LEFT_TOP;
}

static bool screen_available(screen_t candidate, bool date_valid, const app_bme280_readings_t* readings) {
    if (candidate == SCREEN_TIME) {
        return true;
    }
    if (candidate == SCREEN_DATE) {
        return date_valid;
    }
    if (readings == NULL) {
        return false;
    }
    return candidate != SCREEN_TEMPERATURE || readings->temperature_centi_c >= 0
           || selected.layout->points[numeric_top_point()] != NIXIE_CHANNEL_UNASSIGNED;
}

void app_display_step(int8_t direction) {
    if (!initialized || !enabled || selected.layout == NULL || (direction != -1 && direction != 1)) {
        return;
    }
    uint32_t now = bsp_time_ms();
    app_bme280_readings_t readings;
    const app_bme280_readings_t* sample = app_bme280_get(&readings) ? &readings : NULL;
    app_clock_datetime_t datetime;
    bool time_valid = false;
    bool date_valid = read_clock(&datetime, &time_valid) && time_valid && app_clock_datetime_is_valid(&datetime);
    if (!screen_available(screen, date_valid, sample)) {
        screen = SCREEN_TIME;
    }
    for (uint8_t i = 0U; i < SCREEN_COUNT; ++i) {
        screen = direction > 0 ? (screen_t)((screen + 1U) % SCREEN_COUNT)
                               : (screen_t)((screen + SCREEN_COUNT - 1U) % SCREEN_COUNT);
        if (screen_available(screen, date_valid, sample)) {
            break;
        }
    }
    manual = true;
    screen_started_ms = now;
    last_poll_ms = now - APP_DISPLAY_POLL_MS;
}

static void set_point(uint8_t* symbols, uint8_t point) {
    uint8_t e = selected.layout->points[point];
    if (e != NIXIE_CHANNEL_UNASSIGNED) {
        symbols[e] = NIXIE_SYMBOL_ON;
    }
}

static void put_pair(uint8_t* symbols, uint8_t position, uint8_t value) {
    symbols[selected.layout->digits[position]] = value / 10U;
    symbols[selected.layout->digits[position + 1U]] = value % 10U;
}

static uint16_t render_time(uint8_t* symbols, const app_clock_datetime_t* datetime, bool time_valid) {
    uint16_t rtc_anodes = 0U;
    if (time_valid) {
        put_pair(symbols, 0U, datetime->hour);
        put_pair(symbols, 2U, datetime->minute);
        if (selected.layout->digit_count == 6U) {
            put_pair(symbols, 4U, datetime->second);
        }
        for (uint8_t p = 0U; p < APP_DISPLAY_POINT_COUNT; ++p) {
            uint8_t e = selected.layout->points[p];
            if (e != NIXIE_CHANNEL_UNASSIGNED) {
                set_point(symbols, p);
                rtc_anodes |= (uint16_t)(1U << selected.board.elements[e].anode_channel);
            }
        }
    }
    return rtc_anodes;
}

static bool put_unsigned(uint8_t* symbols, uint32_t value, uint8_t end) {
    do {
        if (end == 0U) {
            return false;
        }
        symbols[selected.layout->digits[--end]] = (uint8_t)(value % 10U);
        value /= 10U;
    } while (value != 0U);
    return true;
}

/* Fractional input is in tenths. Select an internal dot on the correct side
 * of its digit, then fall back to the last external lower point. */
static bool render_number(uint8_t* symbols, uint16_t* dots, uint32_t value, bool fractional, bool negative) {
    uint8_t count = selected.layout->digit_count;
    uint8_t position = count - 2U;
    uint8_t e = selected.layout->digits[position + 1U];
    const nixie_tube_t* tube = selected.board.elements[e].tube;
    bool internal = tube->dot_before_digit && (available_symbols(tube) & (1U << NIXIE_SYMBOL_DOT)) != 0U;
    if (!internal) {
        e = selected.layout->digits[position];
        tube = selected.board.elements[e].tube;
        internal = !tube->dot_before_digit && (available_symbols(tube) & (1U << NIXIE_SYMBOL_DOT)) != 0U;
    }
    uint8_t bottom = numeric_top_point() + 1U;
    if (fractional && !internal) {
        position = count - 3U;
        uint32_t limit = 1U;
        for (uint8_t i = 0U; i <= position; ++i) {
            limit *= 10U;
        }
        if (selected.layout->points[bottom] == NIXIE_CHANNEL_UNASSIGNED || value / 10U >= limit) {
            value = (value + 5U) / 10U;
            fractional = false;
        }
    }
    if (fractional) {
        if (!put_unsigned(symbols, value / 10U, position + 1U)) {
            return false;
        }
        symbols[selected.layout->digits[position + 1U]] = (uint8_t)(value % 10U);
        if (internal) {
            *dots |= (uint16_t)(1U << e);
        } else {
            set_point(symbols, bottom);
        }
    } else if (!put_unsigned(symbols, value, count)) {
        return false;
    }
    if (negative && value != 0U) {
        set_point(symbols, numeric_top_point());
    }
    return true;
}

static void present_ui(uint32_t now) {
    app_clock_datetime_t datetime;
    bool time_valid;
    if (!read_clock(&datetime, &time_valid)) {
        return;
    }
    bool date_valid = time_valid && app_clock_datetime_is_valid(&datetime);
    app_bme280_readings_t readings;
    const app_bme280_readings_t* sample = app_bme280_get(&readings) ? &readings : NULL;
    uint32_t duration = manual ? APP_DISPLAY_MANUAL_MS
                               : (screen == SCREEN_TIME ? APP_DISPLAY_TIME_MS : APP_DISPLAY_INFO_MS);
    if ((uint32_t)(now - screen_started_ms) >= duration
        || (manual && !screen_available(screen, date_valid, sample))) {
        screen = manual ? SCREEN_TIME : (screen_t)((screen + 1U) % SCREEN_COUNT);
        manual = false;
        screen_started_ms = now;
    }
    screen_t shown = screen_available(screen, date_valid, sample) ? screen : SCREEN_TIME;
    uint8_t symbols[NIXIE_MAX_ELEMENTS];
    memset(symbols, NIXIE_SYMBOL_BLANK, selected.board.element_count);
    uint16_t dots = 0U;
    uint16_t rtc_anodes = 0U;
    bool formatted = true;
    switch (shown) {
        case SCREEN_DATE:
            put_pair(symbols, 0U, datetime.day);
            put_pair(symbols, 2U, datetime.month);
            set_point(symbols, APP_DISPLAY_POINT_LEFT_BOTTOM);
            if (selected.layout->digit_count == 6U) {
                put_pair(symbols, 4U, (uint8_t)(datetime.year % 100U));
                set_point(symbols, APP_DISPLAY_POINT_RIGHT_BOTTOM);
            }
            break;
        case SCREEN_TEMPERATURE: {
            int32_t t = sample->temperature_centi_c;
            uint32_t magnitude = t < 0 ? 0U - (uint32_t)t : (uint32_t)t;
            formatted = render_number(symbols, &dots, (magnitude + 5U) / 10U, true, t < 0);
            break;
        }
        case SCREEN_HUMIDITY:
            formatted = render_number(symbols, &dots, (sample->humidity_centi_percent + 5U) / 10U, true, false);
            break;
        case SCREEN_PRESSURE:
            /* Integer mmHg, using 133.322 Pa/mmHg. Bosch pressure is Pa * 100;
             * the bounded sensor range keeps this multiplication within uint32. */
            formatted = render_number(symbols, &dots, (sample->pressure_centi_pa * 10U + 66661U) / 133322U,
                                      false, false);
            break;
        default: rtc_anodes = render_time(symbols, &datetime, time_valid); break;
    }
    if (!formatted) {
        memset(symbols, NIXIE_SYMBOL_BLANK, selected.board.element_count);
        dots = 0U;
        rtc_anodes = render_time(symbols, &datetime, time_valid);
    }
    /* Compare every accepted part, including the gate: identical digits can
     * belong to screens with different point behavior. BUSY retries next poll. */
    if (dots != current_dots || rtc_anodes != current_rtc_anodes
        || memcmp(symbols, current_symbols, selected.board.element_count) != 0) {
        (void)publish_configuration(&selected, symbols, dots, rtc_anodes);
    }
}

void app_display_process(bool power_ready) {
    if (!initialized) {
        return;
    }
    if (power_ready != enabled && bsp_nixie_set_enabled(power_ready)) {
        enabled = power_ready;
        if (enabled) {
            screen = SCREEN_TIME;
            manual = false;
            screen_started_ms = bsp_time_ms();
            last_poll_ms = screen_started_ms - APP_DISPLAY_POLL_MS;
        }
    }
    uint32_t now = bsp_time_ms();
    if (selected.layout == NULL || (uint32_t)(now - last_poll_ms) < APP_DISPLAY_POLL_MS) {
        return;
    }
    last_poll_ms = now;
    if (enabled) {
        present_ui(now);
    }
}
