#ifndef BSP_NIXIE_H
#define BSP_NIXIE_H

#include "nixie.h"

typedef struct {
    uint32_t frames;
    uint32_t missed_slots;         /* Phase IRQs delayed by at least one nominal scan slot. */
    uint32_t refresh_rate_millihz; /* Nominal plan rate; IRQ overhead extends the frame. */
    uint16_t max_irq_delay_us;     /* Cumulative maximum compare-IRQ lateness, modulo 65536 us. */
    bool running;
    bool separators_on; /* Commanded static outputs; not a discharge measurement. */
    bool shutdown;
} bsp_nixie_status_t;

/* Call after bsp_board_init(). Public operations are serialized in foreground.
 * TIM1 and EXTI13 belong to this backend; RTC still drives PC13.
 * No DMA channels are used. */
bool bsp_nixie_init(void);
const nixie_caps_t* bsp_nixie_caps(void);
/* Includes generic validation and rejects every electrode reserved for static
 * separators (A2; C3, C7, C10, C13), including currently blank symbols. */
nixie_status_t bsp_nixie_validate_board(const nixie_board_t* board, nixie_issue_t* issue);
/* Copies the digit plan and publishes it at a frame boundary. Reserved static
 * channels are rejected. The pointer is borrowed only for this call.
 * Invalid/BUSY leaves the accepted state unchanged; retry BUSY in foreground. */
nixie_status_t bsp_nixie_present(const nixie_plan_t* plan);
/* Common HV permission: false stops scanning AND blanks static separators. */
bool bsp_nixie_set_enabled(bool enabled);
/* Independent separator permission (valid clock). With HV permission, all four
 * stay continuously on while PC13 is LOW; EXTI tracks both RTC edges. */
bool bsp_nixie_set_separators(bool enabled);
bool bsp_nixie_get_status(bsp_nixie_status_t* status);
/* Latched emergency stop, valid before board init and from fault context.
 * Synchronously stops TIM1 and drives every anode/cathode LOW. */
void bsp_nixie_shutdown(void);
/* TIM1 CC vector dispatch. Only CC1 is enabled; update does not end pulses. */
void bsp_nixie_timer_irq(void);
/* EXTI4_15 dispatch; handles only the PC13 RTC phase line. */
void bsp_nixie_separator_irq(void);

#endif /* BSP_NIXIE_H */
