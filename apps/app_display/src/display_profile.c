#include "display_profile.h"

/* EDIT THIS FILE to describe the actual upper board. All channel numbers refer
 * to ANODE_n/CATHODE_n in .ioc, NOT to GPIO bits or tube package pin numbers.
 * UNASSIGNED is deliberately invalid: do not replace it with guessed wiring. */

/* Local cathodes 0..9 mean digit electrodes, index 10 means the IN-12B comma.
 * The first ten entries are also the complete symbol set of IN-12A. */
static const nixie_symbol_t in12_symbols[] = {
    {0U, 0U},
    {1U, 1U},
    {2U, 2U},
    {3U, 3U},
    {4U, 4U},
    {5U, 5U},
    {6U, 6U},
    {7U, 7U},
    {8U, 8U},
    {9U, 9U},
    {NIXIE_SYMBOL_DOT, 10U},
};

const nixie_tube_t display_tube_in12a = {
    .name = "IN-12A",
    .cathode_count = 10U,
    .symbols = in12_symbols,
    .symbol_count = 10U,
};

const nixie_tube_t display_tube_in12b = {
    .name = "IN-12B",
    .cathode_count = 11U,
    .symbols = in12_symbols,
    .symbol_count = 11U,
};

static const nixie_symbol_t separator_symbols[] = {{NIXIE_SYMBOL_ON, 0U}};
static const nixie_tube_t separator = {
    .name = "INS-1",
    .cathode_count = 1U,
    .symbols = separator_symbols,
    .symbol_count = 1U,
};

/* REQUIRED: CATHODE_n connected to digits 0,1,2,3,4,5,6,7,8,9,comma respectively.
 * Share this table only if all six sockets really have the same connections.
 * For different wiring, make another table and point the relevant socket at it.
 * IN-12A uses the first ten entries and has no internal comma. */
static const uint8_t in12_cathode_channels[] = {
    12, /* digit 0 */
    14, /* digit 1 */
    11, /* digit 2 */
    6,  /* digit 3 */
    5,  /* digit 4 */
    1,  /* digit 5 */
    0,  /* digit 6 */
    2,  /* digit 7 */
    4,  /* digit 8 */
    9,  /* digit 9 */
    15, /* internal comma, IN-12B only */
};

/* Four separately ballasted points share A2 and one scan slot. */
static const uint8_t separator_channels[] = {3U, 13U, 7U, 10U};

static const nixie_element_t upper_board[] = {
    {.tube = &display_tube_in12b, .anode_channel = 6, .cathode_channels = in12_cathode_channels}, /* digit, leftmost */
    {.tube = &display_tube_in12b, .anode_channel = 5, .cathode_channels = in12_cathode_channels},
    {.tube = &display_tube_in12b, .anode_channel = 4, .cathode_channels = in12_cathode_channels},
    {.tube = &display_tube_in12b, .anode_channel = 3, .cathode_channels = in12_cathode_channels},
    {.tube = &display_tube_in12b, .anode_channel = 1, .cathode_channels = in12_cathode_channels},
    {.tube = &display_tube_in12b, .anode_channel = 0, .cathode_channels = in12_cathode_channels}, /* digit, rightmost */
    {.tube = &separator, .anode_channel = 2, .cathode_channels = &separator_channels[0]},
    {.tube = &separator, .anode_channel = 2, .cathode_channels = &separator_channels[1]},
    {.tube = &separator, .anode_channel = 2, .cathode_channels = &separator_channels[2]},
    {.tube = &separator, .anode_channel = 2, .cathode_channels = &separator_channels[3]},
};

/* Clock presentation follows logical elements, independently of GPIO wiring.
 * A four-digit board uses only HOUR/MINUTE roles. Omit absent elements; use
 * UNUSED for elements which should stay blank. Separator anodes follow PC13. */
static const app_display_clock_role_t clock_roles[] = {
    [0] = APP_DISPLAY_CLOCK_HOUR_TENS,    [1] = APP_DISPLAY_CLOCK_HOUR_UNITS,  [2] = APP_DISPLAY_CLOCK_MINUTE_TENS,
    [3] = APP_DISPLAY_CLOCK_MINUTE_UNITS, [4] = APP_DISPLAY_CLOCK_SECOND_TENS, [5] = APP_DISPLAY_CLOCK_SECOND_UNITS,
    [6] = APP_DISPLAY_CLOCK_SEPARATOR, [7] = APP_DISPLAY_CLOCK_SEPARATOR,
    [8] = APP_DISPLAY_CLOCK_SEPARATOR, [9] = APP_DISPLAY_CLOCK_SEPARATOR,
};

_Static_assert(sizeof(clock_roles) / sizeof(clock_roles[0]) == sizeof(upper_board) / sizeof(upper_board[0]),
               "Every display element must have a clock role");

const app_display_profile_t display_default_profile = {
    .board =
        {
            .name = "6 x IN-12 + 4 INS-1 (shared A2)",
            .elements = upper_board,
            .element_count = sizeof(upper_board) / sizeof(upper_board[0]),
        },
    .clock_roles = clock_roles,
    /* Wait for the TLP127 anode switch to turn off before changing cathodes. */
    .timing =
        {
            .refresh_rate_hz = 120U,
            .blanking_time_us = 500U,
        },
    .brightness_pm = 1000U, /* Global 0..1000, multiplied by the anode factor. */
    /* Per scanned ANODE_n, 0..1000; A2 controls all four separator points.
     * All elements sharing an anode have the same PWM brightness.
     * Entries for unused/unavailable anodes have no effect. */
    .anode_brightness_pm =
        {
            [0] = 1000U,
            [1] = 1000U,
            [2] = 1000U,
            [3] = 1000U,
            [4] = 1000U,
            [5] = 1000U,
            [6] = 1000U,
            [7] = 1000U,
        },
};
