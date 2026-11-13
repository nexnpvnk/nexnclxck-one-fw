#ifndef BSP_NIXIE_H
#define BSP_NIXIE_H

#include "nixie.h"

typedef struct {
    uint32_t frames;
    uint32_t missed_slots;         /* Phase IRQs delayed by at least one nominal scan slot. */
    uint32_t refresh_rate_millihz; /* Nominal plan rate; IRQ overhead extends the frame. */
    uint16_t max_irq_delay_us;     /* Cumulative maximum compare-IRQ lateness, modulo 65536 us. */
    bool running;
    bool separators_on; /* RTC gate for lit separator slots; not instantaneous GPIO. */
    bool shutdown;
} bsp_nixie_status_t;

/* Call after bsp_board_init(). Public operations are serialized in foreground.
 * TIM1 belongs to this backend; RTC still drives PC13.
 * No DMA channels are used. */
bool bsp_nixie_init(void);
const nixie_caps_t* bsp_nixie_caps(void);
/* Copies the plan and publishes it at a frame boundary. rtc_anodes selects
 * complete anodes gated by the active-LOW PC13 phase, sampled once per frame.
 * Gated dark slots retain their full duration. Pointer borrowed only for this call.
 * Invalid/BUSY leaves the accepted state unchanged; retry BUSY in foreground. */
nixie_status_t bsp_nixie_present(const nixie_plan_t* plan, uint16_t rtc_anodes);
/* Common HV permission: false stops scanning and blanks every output. */
bool bsp_nixie_set_enabled(bool enabled);
bool bsp_nixie_get_status(bsp_nixie_status_t* status);
/* Latched emergency stop, valid before board init and from fault context.
 * Synchronously stops TIM1 and drives every anode/cathode LOW. */
void bsp_nixie_shutdown(void);
/* TIM1 CC vector dispatch. Only CC1 is enabled; update does not end pulses. */
void bsp_nixie_timer_irq(void);

#endif /* BSP_NIXIE_H */
