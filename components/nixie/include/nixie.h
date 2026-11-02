#ifndef NIXIE_H
#define NIXIE_H

#include <stdbool.h>
#include <stdint.h>

/* One scan slot per distinct wired anode, one selectable electrode per element.
 * Shared-anode elements must have separate current-limiting paths. */
#define NIXIE_MAX_ELEMENTS       16U
#define NIXIE_MAX_ANODES         16U
#define NIXIE_MAX_CATHODES       16U
#define NIXIE_BRIGHTNESS_MAX     1000U

#define NIXIE_CHANNEL_UNASSIGNED UINT8_MAX
/* Only a one-cathode indicator may have a permanently connected cathode. */
#define NIXIE_CHANNEL_FIXED      (UINT8_MAX - 1U)
#define NIXIE_SYMBOL_BLANK       UINT8_MAX
#define NIXIE_SYMBOL_DOT         10U
#define NIXIE_SYMBOL_ON          12U

typedef enum {
    NIXIE_OK = 0,
    NIXIE_INVALID_ARGUMENT,
    NIXIE_INVALID_TUBE,
    NIXIE_INVALID_WIRING,
    NIXIE_INVALID_SYMBOL,
    NIXIE_INVALID_TIMING,
    NIXIE_BUSY,
    NIXIE_NOT_READY,
} nixie_status_t;

typedef struct {
    uint8_t id;      /* 0..9 are digits; other IDs are application-defined. */
    uint8_t cathode; /* Local cathode index in cathode_channels/cathode_pins. */
} nixie_symbol_t;

typedef struct {
    const char* name;
    /* Physical pin numbers are optional documentation: 0/NULL means unspecified.
     * They never determine carrier wiring; cathode_channels does that explicitly. */
    uint8_t anode_pin;
    const uint8_t* cathode_pins;
    uint8_t cathode_count;
    const nixie_symbol_t* symbols;
    uint8_t symbol_count;
} nixie_tube_t;

typedef struct {
    const nixie_tube_t* tube;
    /* Same-anode elements light together and need independent current limiting
     * and disjoint cathode channels. A fixed return requires an exclusive anode. */
    uint8_t anode_channel;
    /* One entry per local cathode, not per digit or physical package pin. */
    const uint8_t* cathode_channels;
} nixie_element_t;

typedef struct {
    const char* name;
    const nixie_element_t* elements;
    uint8_t element_count;
} nixie_board_t;

typedef struct {
    uint16_t refresh_rate_hz; /* A complete scan of all distinct wired anodes. */
    /* Anodes stay off for this interval before selecting the next cathodes. */
    uint16_t blanking_time_us;
} nixie_timing_t;

typedef struct {
    uint8_t anode_count;
    uint8_t cathode_count;
    uint16_t max_slot_us;
} nixie_caps_t;

typedef struct {
    uint16_t brightness_pm;
    uint16_t anode_brightness_pm[NIXIE_MAX_ANODES]; /* Indexed by carrier ANODE_n. */
    uint8_t symbols[NIXIE_MAX_ELEMENTS];            /* Symbol ID per logical element. */
} nixie_frame_t;

typedef struct {
    uint16_t cathodes; /* Carrier mask; 0 is valid for an exclusive fixed return. */
    uint16_t on_us;    /* Common anode PWM; 0 is dark but still reserves the slot. */
    uint8_t anode;
} nixie_slot_t;

typedef struct {
    nixie_timing_t timing;
    uint16_t slot_us;
    uint8_t slot_count; /* Distinct wired anodes, independent of the frame content. */
    nixie_slot_t slots[NIXIE_MAX_ANODES];
} nixie_plan_t;

typedef struct {
    nixie_status_t status;
    /* UINT8_MAX means the error is not associated with a specific element. */
    uint8_t element;
} nixie_issue_t;

/* All functions are foreground-only, have no hardware side effects and allocate
 * no memory. Descriptors are borrowed only for the duration of each call. */
nixie_status_t nixie_validate_board(const nixie_board_t* board, const nixie_caps_t* caps, nixie_issue_t* issue);
/* A failed compilation may modify plan; never publish it unless NIXIE_OK. */
nixie_status_t nixie_compile(const nixie_board_t* board, const nixie_timing_t* timing, const nixie_frame_t* frame,
                             const nixie_caps_t* caps, nixie_plan_t* plan, nixie_issue_t* issue);
nixie_status_t nixie_validate_plan(const nixie_plan_t* plan, const nixie_caps_t* caps);
const char* nixie_status_name(nixie_status_t status);

#endif /* NIXIE_H */
