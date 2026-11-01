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

/* Optional package-pin metadata. Fill from your lamp documentation if desired.
 * Zero entries are unspecified and are NOT used to infer carrier connections. */
static const uint8_t in12_package_cathodes[] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};

const nixie_tube_t display_tube_in12a = {
    .name = "IN-12A",
    .anode_pin = 0U,
    .cathode_pins = in12_package_cathodes,
    .cathode_count = 10U,
    .symbols = in12_symbols,
    .symbol_count = 10U,
};

const nixie_tube_t display_tube_in12b = {
    .name = "IN-12B",
    .anode_pin = 0U,
    .cathode_pins = in12_package_cathodes,
    .cathode_count = 11U,
    .symbols = in12_symbols,
    .symbol_count = 11U,
};

static const nixie_symbol_t separator_symbols[] = {{NIXIE_SYMBOL_ON, 0U}};
static const nixie_tube_t separator = {
    .name = "External separator",
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

/* One cathode channel per separately controlled point, each with its own
 * current-limiting resistor. All selected points can light together on A2. */
static const uint8_t separator_left_channels[] = {3, 13};
static const uint8_t separator_right_channels[] = {7, 10};

/* Logical elements: digits 0..5, left pair 6..7, right pair 8..9.
 * The four points share ANODE_2 and its PWM brightness, with individual on/off.
 * Other boards may assign a separate anode to each point or pair: change each
 * anode_channel accordingly. Shared-anode elements must have disjoint cathodes.
 * Points wired to the same anode AND cathode form one controlled element. */
static const nixie_element_t upper_board[] = {
    {.tube = &display_tube_in12b, .anode_channel = 0, .cathode_channels = in12_cathode_channels}, /* digit, leftmost */
    {.tube = &display_tube_in12b, .anode_channel = 1, .cathode_channels = in12_cathode_channels},
    {.tube = &display_tube_in12b, .anode_channel = 3, .cathode_channels = in12_cathode_channels},
    {.tube = &display_tube_in12b, .anode_channel = 4, .cathode_channels = in12_cathode_channels},
    {.tube = &display_tube_in12b, .anode_channel = 5, .cathode_channels = in12_cathode_channels},
    {.tube = &display_tube_in12b, .anode_channel = 6, .cathode_channels = in12_cathode_channels}, /* digit, rightmost */
    {.tube = &separator, .anode_channel = 2, .cathode_channels = &separator_left_channels[0]},
    {.tube = &separator, .anode_channel = 2, .cathode_channels = &separator_left_channels[1]},
    {.tube = &separator, .anode_channel = 2, .cathode_channels = &separator_right_channels[0]},
    {.tube = &separator, .anode_channel = 2, .cathode_channels = &separator_right_channels[1]},
};

/* Clockwise, assuming each pair is listed upper point first:
 *   6 (left upper)    8 (right upper)
 *   7 (left lower)    9 (right lower)
 * Reorder these indices if the physical point arrangement differs. */
static const uint8_t demo_point_order[] = {6U, 8U, 9U, 7U};

const app_display_profile_t display_default_profile = {
    .board =
        {
            .name = "6 x IN-12 + 4 separator points (shared A2)",
            .elements = upper_board,
            .element_count = sizeof(upper_board) / sizeof(upper_board[0]),
        },
    /* Wait for the TLP127 anode switch to turn off before changing cathodes. */
    .timing =
        {
            .refresh_rate_hz = 240U,
            .blanking_time_us = 500U,
        },
    .brightness_pm = 1000U, /* Global 0..1000, multiplied by the anode factor. */
    /* Per ANODE_n, 0..1000. Entry [2] controls all four external points.
     * All elements sharing an anode have the same PWM brightness.
     * Entries for unused/unavailable anodes have no effect. */
    .anode_brightness_pm =
        {
            [0] = 1000U,
            [1] = 1000U,
            [2] = 1000U, /* All external points on A2. */
            [3] = 1000U,
            [4] = 1000U,
            [5] = 1000U,
            [6] = 1000U,
            [7] = 1000U,
        },
    .demo_step_ms = 1000U,
    .demo_point_order = demo_point_order,
    .demo_point_count = sizeof(demo_point_order) / sizeof(demo_point_order[0]),
};
