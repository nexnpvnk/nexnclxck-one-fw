#ifndef NIXIE_H
#define NIXIE_H

#include <stdbool.h>
#include <stdint.h>

/* Logical elements and scan slots are independent of physical anode count.
 * Keep the slot budget bounded without allocating three slots for every element. */
#define NIXIE_MAX_ELEMENTS       16U
#define NIXIE_MAX_CATHODES       16U
#define NIXIE_MAX_GROUPS         3U
#define NIXIE_MAX_SLOTS          24U
#define NIXIE_BRIGHTNESS_MAX     1000U

#define NIXIE_CHANNEL_UNASSIGNED UINT8_MAX
/* Only a one-cathode indicator may have a permanently connected cathode. */
#define NIXIE_CHANNEL_FIXED      (UINT8_MAX - 1U)
#define NIXIE_SYMBOL_BLANK       UINT8_MAX
#define NIXIE_SYMBOL_DOT         10U
#define NIXIE_SYMBOL_DOT_RIGHT   11U
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
    uint8_t id;        /* 0..9 are digits; other IDs are application-defined. */
    uint8_t group;     /* Alternatives in one group share a slot. */
    uint16_t cathodes; /* Bits refer to the tube's local cathode indices. */
} nixie_symbol_t;

typedef struct {
    const char* name;
    /* Physical pin numbers are optional documentation: 0/NULL means unspecified.
     * They never determine carrier wiring; cathode_channels does that explicitly. */
    uint8_t anode_pin;
    const uint8_t* cathode_pins;
    uint8_t cathode_count;
    uint8_t group_count;
    const nixie_symbol_t* symbols;
    uint8_t symbol_count;
} nixie_tube_t;

typedef struct {
    const nixie_tube_t* tube;
    /* Shared anodes are allowed when the elements have disjoint cathode channels.
     * A fixed-return element must have exclusive use of its anode. */
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
    uint16_t refresh_hz; /* A complete scan, including every reserved group slot. */
    /* Anodes stay off for this interval before cathodes are changed. */
    uint16_t dead_time_us;
} nixie_timing_t;

typedef struct {
    uint8_t anode_count;
    uint8_t cathode_count;
    uint16_t max_slot_us;
} nixie_caps_t;

typedef struct {
    uint16_t brightness_pm;
    uint8_t symbol;
} nixie_cell_t;

typedef struct {
    uint16_t brightness_pm;
    nixie_cell_t cells[NIXIE_MAX_ELEMENTS][NIXIE_MAX_GROUPS];
} nixie_frame_t;

typedef struct {
    uint16_t cathodes; /* Logical carrier channels; 0 is valid for fixed return. */
    uint16_t on_us;    /* 0 means blank; this slot still occupies time. */
    uint8_t anode;
} nixie_slot_t;

typedef struct {
    nixie_timing_t timing;
    uint16_t slot_us;
    uint8_t slot_count;
    nixie_slot_t slots[NIXIE_MAX_SLOTS];
} nixie_plan_t;

typedef struct {
    nixie_status_t status;
    /* UINT8_MAX means the error is not associated with a specific cell. */
    uint8_t element;
    uint8_t group;
} nixie_issue_t;

/* All functions are foreground-only, have no hardware side effects and allocate
 * no memory. Descriptors are borrowed only for the duration of each call. */
void nixie_frame_clear(nixie_frame_t* frame);
nixie_status_t nixie_validate_board(const nixie_board_t* board, const nixie_caps_t* caps, nixie_issue_t* issue);
/* A failed compilation may modify plan; never publish it unless NIXIE_OK. */
nixie_status_t nixie_compile(const nixie_board_t* board, const nixie_timing_t* timing, const nixie_frame_t* frame,
                             const nixie_caps_t* caps, nixie_plan_t* plan, nixie_issue_t* issue);
nixie_status_t nixie_validate_plan(const nixie_plan_t* plan, const nixie_caps_t* caps);
const char* nixie_status_name(nixie_status_t status);

#endif /* NIXIE_H */
