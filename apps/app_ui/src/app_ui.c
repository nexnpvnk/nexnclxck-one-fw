#include "app_ui.h"

#include "app_bme280.h"
#include "app_clock.h"
#include "app_display.h"
#include "app_light.h"
#include "app_sound.h"
#include "bsp_time.h"

#include <stddef.h>
#include <string.h>

#define APP_UI_TIME_MS                 40000U
#define APP_UI_INFO_MS                 5000U
#define APP_UI_MANUAL_MS               10000U
#define APP_UI_POLL_MS                 20U

#define APP_UI_SETTING_TIMEOUT_MS      10000U
#define APP_UI_SETTING_REPEAT_DELAY_MS 400U

#define APP_UI_BEEP_FREQ_HZ            2800U
#define APP_UI_BEEP_VOLUME             10U
#define APP_UI_BEEP_DURATION_MS        35U

typedef enum {
    SCREEN_TIME = 0,
    SCREEN_DATE,
    SCREEN_TEMPERATURE,
    SCREEN_HUMIDITY,
    SCREEN_PRESSURE,
    SCREEN_COUNT,
} screen_t;

typedef enum {
    SETTING_NONE = 0,
    SETTING_LIGHT_BRIGHTNESS,
    SETTING_LIGHT_SPEED,
    SETTING_LIGHT_PRIMARY,
    SETTING_LIGHT_SECONDARY,
    SETTING_COUNT,
} setting_id_t;

typedef struct {
    uint8_t minimum;
    uint8_t maximum;
    bool wrap;
} setting_range_t;

static const ledfx_rgb_t light_palette[] = {
    {255U, 80U, 8U},    /*  1: warm orange, startup primary */
    {255U, 144U, 0U},   /*  2: amber */
    {255U, 208U, 0U},   /*  3: yellow */
    {128U, 255U, 0U},   /*  4: lime */
    {0U, 255U, 0U},     /*  5: green */
    {0U, 255U, 128U},   /*  6: turquoise */
    {0U, 255U, 255U},   /*  7: cyan */
    {0U, 32U, 255U},    /*  8: blue, startup secondary */
    {128U, 0U, 255U},   /*  9: violet */
    {255U, 0U, 255U},   /* 10: magenta */
    {255U, 64U, 128U},  /* 11: pink */
    {255U, 0U, 0U},     /* 12: red */
    {255U, 214U, 160U}, /* 13: warm white */
    {255U, 255U, 255U}, /* 14: white */
    {192U, 216U, 255U}, /* 15: cool white */
    {0U, 0U, 0U},       /* 16: black */
};

#define APP_UI_LIGHT_COLOR_COUNT (sizeof(light_palette) / sizeof(light_palette[0]))

/* One numeric editor owns stepping, repeat, timeout and presentation.
 * Setting adapters below only read/write application values. */
static const setting_range_t setting_ranges[SETTING_COUNT] = {
    [SETTING_LIGHT_BRIGHTNESS] = {0U, UINT8_MAX, false},
    [SETTING_LIGHT_SPEED] = {0U, UINT8_MAX, false},
    [SETTING_LIGHT_PRIMARY] = {1U, APP_UI_LIGHT_COLOR_COUNT, true},
    [SETTING_LIGHT_SECONDARY] = {1U, APP_UI_LIGHT_COLOR_COUNT, true},
};

static const nixie_board_t* board;
static const app_display_layout_t* layout;
static screen_t screen;
static uint32_t screen_started_ms;
static uint32_t last_poll_ms;
static bool initialized;
static bool view_started;
static bool manual;
static setting_id_t setting;
static bool fn_short_pending;
static int8_t adjustment_direction;
static uint32_t setting_activity_ms;
static uint32_t setting_hold_started_ms;
static uint32_t setting_last_step_ms;

static nixie_status_t report(nixie_issue_t* issue, nixie_status_t status, uint8_t element) {
    if (issue != NULL) {
        *issue = (nixie_issue_t){status, element};
    }
    return status;
}

static uint16_t available_symbols(const nixie_tube_t* tube) {
    uint16_t mask = 0U;
    for (uint8_t s = 0U; s < tube->symbol_count; ++s) {
        if (tube->symbols[s].id < 16U) {
            mask |= (uint16_t)(1U << tube->symbols[s].id);
        }
    }
    return mask;
}

static nixie_status_t validate_layout(
    const nixie_board_t* board,
    const app_display_layout_t* layout,
    nixie_issue_t* issue
) {
    if (layout == NULL) {
        return report(issue, NIXIE_INVALID_ARGUMENT, UINT8_MAX);
    }
    if ((layout->digit_count != 4U && layout->digit_count != 6U) ||
        (layout->digit_count == 4U && (layout->points[APP_DISPLAY_POINT_RIGHT_TOP] != NIXIE_CHANNEL_UNASSIGNED ||
                                       layout->points[APP_DISPLAY_POINT_RIGHT_BOTTOM] != NIXIE_CHANNEL_UNASSIGNED))) {
        return report(issue, NIXIE_INVALID_ARGUMENT, UINT8_MAX);
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
        if (e >= board->element_count || (used & (1U << e)) != 0U) {
            return report(issue, NIXIE_INVALID_ARGUMENT, e);
        }
        used |= (uint16_t)(1U << e);
        uint16_t anode = (uint16_t)(1U << board->elements[e].anode_channel);
        if (digit) {
            digit_anodes |= anode;
        } else {
            separator_anodes |= anode;
        }
        if ((separator_anodes & digit_anodes) != 0U) {
            return report(issue, NIXIE_INVALID_WIRING, e);
        }
        uint16_t required = digit ? 0x3FFU : (1U << NIXIE_SYMBOL_ON);
        if ((available_symbols(board->elements[e].tube) & required) != required) {
            return report(issue, NIXIE_INVALID_SYMBOL, e);
        }
    }
    return report(issue, NIXIE_OK, UINT8_MAX);
}

nixie_status_t app_ui_init(nixie_issue_t* issue) {
    initialized = false;
    view_started = false;
    const app_display_profile_t* profile = app_display_profile();
    app_display_status_t display_status;
    if (!app_display_get_status(&display_status)) {
        return report(issue, NIXIE_NOT_READY, UINT8_MAX);
    }
    nixie_status_t result = validate_layout(&profile->board, profile->layout, issue);
    if (result != NIXIE_OK) {
        return result;
    }
    board = &profile->board;
    layout = profile->layout;
    screen = SCREEN_TIME;
    manual = false;
    setting = SETTING_NONE;
    fn_short_pending = false;
    adjustment_direction = 0;
    (void)app_light_set_preview(APP_LIGHT_PREVIEW_NONE);
    screen_started_ms = bsp_time_ms();
    last_poll_ms = screen_started_ms - APP_UI_POLL_MS;
    initialized = true;
    return NIXIE_OK;
}

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
    return layout->digit_count == 6U ? APP_DISPLAY_POINT_RIGHT_TOP : APP_DISPLAY_POINT_LEFT_TOP;
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
    return candidate != SCREEN_TEMPERATURE ||
           readings->temperature_centi_c >= 0 ||
           layout->points[numeric_top_point()] != NIXIE_CHANNEL_UNASSIGNED;
}

static void step_screen(int8_t direction) {
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
    view_started = true;
    screen_started_ms = now;
    last_poll_ms = now - APP_UI_POLL_MS;
}

static void finish_adjustment(uint32_t now) {
    setting = SETTING_NONE;
    adjustment_direction = 0;
    fn_short_pending = false;
    (void)app_light_set_preview(APP_LIGHT_PREVIEW_NONE);
    screen = SCREEN_TIME;
    manual = false;
    view_started = true;
    screen_started_ms = now;
    last_poll_ms = now - APP_UI_POLL_MS;
}

static void step_light_effect(void) {
    app_light_status_t status;
    if (app_light_get_status(&status)) {
        ledfx_effect_t next = (ledfx_effect_t)((status.effect + 1U) % LEDFX_EFFECT_COUNT);
        (void)app_light_set_effect(next);
    }
}

static uint8_t palette_index(ledfx_rgb_t color) {
    for (uint8_t i = 0U; i < APP_UI_LIGHT_COLOR_COUNT; ++i) {
        if (light_palette[i].r == color.r && light_palette[i].g == color.g && light_palette[i].b == color.b) {
            return i + 1U;
        }
    }
    return 0U; /* Custom RGB from the CLI; do not change it just by entering. */
}

static bool read_setting_value(setting_id_t id, uint8_t* value) {
    app_light_status_t status;
    if (!app_light_get_status(&status)) {
        return false;
    }
    switch (id) {
        case SETTING_LIGHT_BRIGHTNESS:
            *value = status.brightness;
            break;
        case SETTING_LIGHT_SPEED:
            *value = status.speed;
            break;
        case SETTING_LIGHT_PRIMARY:
            *value = palette_index(status.primary);
            break;
        case SETTING_LIGHT_SECONDARY:
            *value = palette_index(status.secondary);
            break;
        default:
            return false;
    }
    return true;
}

static bool write_setting_value(setting_id_t id, uint8_t value) {
    if (id == SETTING_LIGHT_BRIGHTNESS) {
        return app_light_set_brightness(value);
    }
    if (id == SETTING_LIGHT_SPEED) {
        return app_light_set_speed(value);
    }
    app_light_status_t status;
    if ((id != SETTING_LIGHT_PRIMARY && id != SETTING_LIGHT_SECONDARY) ||
        value == 0U ||
        value > APP_UI_LIGHT_COLOR_COUNT ||
        !app_light_get_status(&status)) {
        return false;
    }
    if (id == SETTING_LIGHT_PRIMARY) {
        status.primary = light_palette[value - 1U];
    } else {
        status.secondary = light_palette[value - 1U];
    }
    return app_light_set_colors(status.primary, status.secondary);
}

static void next_setting(uint32_t now) {
    setting_id_t next = (setting_id_t)(setting + 1U);
    if (next == SETTING_COUNT) {
        next = SETTING_LIGHT_BRIGHTNESS;
    }
    uint8_t value;
    if (!read_setting_value(next, &value)) {
        return;
    }
    app_light_preview_t preview = APP_LIGHT_PREVIEW_NONE;
    if (next == SETTING_LIGHT_PRIMARY) {
        preview = APP_LIGHT_PREVIEW_PRIMARY;
    } else if (next == SETTING_LIGHT_SECONDARY) {
        preview = APP_LIGHT_PREVIEW_SECONDARY;
    }
    if (!app_light_set_preview(preview)) {
        return;
    }
    setting = next;
    adjustment_direction = 0;
    setting_activity_ms = now;
    view_started = true;
    last_poll_ms = now - APP_UI_POLL_MS;
}

static void step_setting(int8_t direction) {
    uint8_t value;
    if (!read_setting_value(setting, &value)) {
        return;
    }
    const setting_range_t* range = &setting_ranges[setting];
    if (value < range->minimum || value > range->maximum) {
        value = direction > 0 ? range->minimum : range->maximum;
    } else if (direction > 0) {
        value = value < range->maximum ? value + 1U : (range->wrap ? range->minimum : range->maximum);
    } else {
        value = value > range->minimum ? value - 1U : (range->wrap ? range->maximum : range->minimum);
    }
    (void)write_setting_value(setting, value);
}

static void repeat_adjustment(uint32_t now) {
    if (adjustment_direction == 0) {
        return;
    }
    /* Holding a key is activity even at the bounds. Keep unit steps;
     * accelerate only their rate and never replay missed steps in a burst. */
    setting_activity_ms = now;
    uint32_t held_ms = now - setting_hold_started_ms;
    uint32_t interval_ms = held_ms >= 2500U ? 20U : (held_ms >= 1200U ? 60U : 150U);
    if (held_ms >= APP_UI_SETTING_REPEAT_DELAY_MS && (uint32_t)(now - setting_last_step_ms) >= interval_ms) {
        step_setting(adjustment_direction);
        setting_last_step_ms = now;
    }
}

void app_ui_handle_input(app_ui_key_t key, app_ui_event_t event) {
    if (!initialized || (unsigned)key >= APP_UI_KEY_COUNT) {
        return;
    }
    uint32_t now = bsp_time_ms();
    switch (event) {
        case APP_UI_EVENT_PRESS:
            if (setting != SETTING_NONE) {
                setting_activity_ms = now;
            }
            if (key == APP_UI_KEY_FN) {
                fn_short_pending = true;
            } else if (setting != SETTING_NONE) {
                if (key == APP_UI_KEY_RIGHT || key == APP_UI_KEY_LEFT) {
                    adjustment_direction = key == APP_UI_KEY_RIGHT ? 1 : -1;
                    setting_hold_started_ms = now;
                    setting_last_step_ms = now;
                    step_setting(adjustment_direction);
                } else if (key == APP_UI_KEY_OK) {
                    finish_adjustment(now);
                }
            } else if (key == APP_UI_KEY_RIGHT) {
                step_screen(1);
            } else if (key == APP_UI_KEY_LEFT) {
                step_screen(-1);
            }
            (void)app_sound_beep(APP_UI_BEEP_FREQ_HZ, APP_UI_BEEP_VOLUME, APP_UI_BEEP_DURATION_MS);
            break;
        case APP_UI_EVENT_LONG_PRESS:
            if (setting != SETTING_NONE && (key == APP_UI_KEY_LEFT || key == APP_UI_KEY_RIGHT)) {
                /* Repeat is handled by process(); no hold beep in adjustment. */
                break;
            }
            if (key == APP_UI_KEY_FN && fn_short_pending) {
                /* A hold enters the editor. Consume RELEASE so it cannot
                 * also switch the effect or advance the first setting. */
                fn_short_pending = false;
                if (setting != SETTING_NONE) {
                    break;
                }
                next_setting(now);
            }
            (void)app_sound_beep(APP_UI_BEEP_FREQ_HZ, APP_UI_BEEP_VOLUME, APP_UI_BEEP_DURATION_MS * 4U);
            break;
        case APP_UI_EVENT_RELEASE:
            if ((key == APP_UI_KEY_RIGHT && adjustment_direction > 0) ||
                (key == APP_UI_KEY_LEFT && adjustment_direction < 0)) {
                adjustment_direction = 0;
                setting_activity_ms = now;
            }
            if (key == APP_UI_KEY_FN && fn_short_pending) {
                fn_short_pending = false;
                if (setting != SETTING_NONE) {
                    next_setting(now);
                } else {
                    step_light_effect();
                }
            }
            break;
        default:
            break;
    }
}

static void set_point(uint8_t* symbols, uint8_t point) {
    uint8_t e = layout->points[point];
    if (e != NIXIE_CHANNEL_UNASSIGNED) {
        symbols[e] = NIXIE_SYMBOL_ON;
    }
}

static void put_pair(uint8_t* symbols, uint8_t position, uint8_t value) {
    symbols[layout->digits[position]] = value / 10U;
    symbols[layout->digits[position + 1U]] = value % 10U;
}

static uint16_t render_time(uint8_t* symbols, const app_clock_datetime_t* datetime, bool time_valid) {
    uint16_t blink_elements = 0U;
    if (time_valid) {
        put_pair(symbols, 0U, datetime->hour);
        put_pair(symbols, 2U, datetime->minute);
        if (layout->digit_count == 6U) {
            put_pair(symbols, 4U, datetime->second);
        }
        for (uint8_t p = 0U; p < APP_DISPLAY_POINT_COUNT; ++p) {
            uint8_t e = layout->points[p];
            if (e != NIXIE_CHANNEL_UNASSIGNED) {
                set_point(symbols, p);
                blink_elements |= (uint16_t)(1U << e);
            }
        }
    }
    return blink_elements;
}

static bool put_unsigned(uint8_t* symbols, uint32_t value, uint8_t end) {
    do {
        if (end == 0U) {
            return false;
        }
        symbols[layout->digits[--end]] = (uint8_t)(value % 10U);
        value /= 10U;
    } while (value != 0U);
    return true;
}

/* Fractional input is in tenths. Select an internal dot on the correct side
 * of its digit, then fall back to the last external lower point. */
static bool render_number(uint8_t* symbols, uint16_t* dots, uint32_t value, bool fractional, bool negative) {
    uint8_t count = layout->digit_count;
    uint8_t position = count - 2U;
    uint8_t e = layout->digits[position + 1U];
    const nixie_tube_t* tube = board->elements[e].tube;
    bool internal = tube->dot_before_digit && (available_symbols(tube) & (1U << NIXIE_SYMBOL_DOT)) != 0U;
    if (!internal) {
        e = layout->digits[position];
        tube = board->elements[e].tube;
        internal = !tube->dot_before_digit && (available_symbols(tube) & (1U << NIXIE_SYMBOL_DOT)) != 0U;
    }
    uint8_t bottom = numeric_top_point() + 1U;
    if (fractional && !internal) {
        position = count - 3U;
        uint32_t limit = 1U;
        for (uint8_t i = 0U; i <= position; ++i) {
            limit *= 10U;
        }
        if (layout->points[bottom] == NIXIE_CHANNEL_UNASSIGNED || value / 10U >= limit) {
            value = (value + 5U) / 10U;
            fractional = false;
        }
    }
    if (fractional) {
        if (!put_unsigned(symbols, value / 10U, position + 1U)) {
            return false;
        }
        symbols[layout->digits[position + 1U]] = (uint8_t)(value % 10U);
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

static void show_setting(void) {
    uint8_t value;
    if (!read_setting_value(setting, &value)) {
        return;
    }
    app_display_frame_t frame = {.element_count = board->element_count};
    memset(frame.symbols, NIXIE_SYMBOL_BLANK, sizeof(frame.symbols));
    frame.symbols[layout->digits[0]] = (uint8_t)setting;
    (void)put_unsigned(frame.symbols, value, layout->digit_count);
    (void)app_display_present(&frame);
}

static void update_view(uint32_t now, bool can_render) {
    app_clock_datetime_t datetime;
    bool time_valid;
    if (!read_clock(&datetime, &time_valid)) {
        return;
    }
    bool date_valid = time_valid && app_clock_datetime_is_valid(&datetime);
    app_bme280_readings_t readings;
    const app_bme280_readings_t* sample = app_bme280_get(&readings) ? &readings : NULL;
    uint32_t duration = manual ? APP_UI_MANUAL_MS : (screen == SCREEN_TIME ? APP_UI_TIME_MS : APP_UI_INFO_MS);
    if ((uint32_t)(now - screen_started_ms) >= duration || (manual && !screen_available(screen, date_valid, sample))) {
        screen = manual ? SCREEN_TIME : (screen_t)((screen + 1U) % SCREEN_COUNT);
        manual = false;
        screen_started_ms = now;
    }
    screen_t shown = screen_available(screen, date_valid, sample) ? screen : SCREEN_TIME;
    if (!can_render) {
        return;
    }
    app_display_frame_t frame = {.element_count = board->element_count};
    memset(frame.symbols, NIXIE_SYMBOL_BLANK, sizeof(frame.symbols));
    bool formatted = true;
    switch (shown) {
        case SCREEN_DATE:
            put_pair(frame.symbols, 0U, datetime.day);
            put_pair(frame.symbols, 2U, datetime.month);
            set_point(frame.symbols, APP_DISPLAY_POINT_LEFT_BOTTOM);
            if (layout->digit_count == 6U) {
                put_pair(frame.symbols, 4U, (uint8_t)(datetime.year % 100U));
                set_point(frame.symbols, APP_DISPLAY_POINT_RIGHT_BOTTOM);
            }
            break;
        case SCREEN_TEMPERATURE: {
            int32_t t = sample->temperature_centi_c;
            uint32_t magnitude = t < 0 ? 0U - (uint32_t)t : (uint32_t)t;
            formatted = render_number(frame.symbols, &frame.dot_elements, (magnitude + 5U) / 10U, true, t < 0);
            break;
        }
        case SCREEN_HUMIDITY:
            formatted = render_number(
                frame.symbols,
                &frame.dot_elements,
                (sample->humidity_centi_percent + 5U) / 10U,
                true,
                false
            );
            break;
        case SCREEN_PRESSURE:
            /* Integer mmHg, using 133.322 Pa/mmHg. Bosch pressure is Pa * 100;
             * the bounded sensor range keeps this multiplication within uint32. */
            formatted = render_number(
                frame.symbols,
                &frame.dot_elements,
                (sample->pressure_centi_pa * 10U + 66661U) / 133322U,
                false,
                false
            );
            break;
        default:
            frame.blink_elements = render_time(frame.symbols, &datetime, time_valid);
            break;
    }
    if (!formatted) {
        memset(frame.symbols, NIXIE_SYMBOL_BLANK, board->element_count);
        frame.dot_elements = 0U;
        frame.blink_elements = render_time(frame.symbols, &datetime, time_valid);
    }
    (void)app_display_present(&frame);
}

void app_ui_process(bool can_render) {
    if (!initialized) {
        return;
    }
    uint32_t now = bsp_time_ms();
    if (setting != SETTING_NONE) {
        repeat_adjustment(now);
        if ((uint32_t)(now - setting_activity_ms) >= APP_UI_SETTING_TIMEOUT_MS) {
            finish_adjustment(now);
        }
    }
    if (!view_started) {
        if (!can_render) {
            return;
        }
        view_started = true;
        screen_started_ms = now;
        last_poll_ms = now - APP_UI_POLL_MS;
    }
    if ((uint32_t)(now - last_poll_ms) < APP_UI_POLL_MS) {
        return;
    }
    last_poll_ms = now;
    if (setting != SETTING_NONE) {
        if (can_render) {
            show_setting();
        }
        return;
    }
    update_view(now, can_render);
}
