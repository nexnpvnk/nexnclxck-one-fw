#ifndef BSP_NIXIE_H
#define BSP_NIXIE_H

#include "nixie.h"

typedef struct {
    uint32_t frames;
    uint32_t missed_slots; /* Cumulative skipped slots and PWM completions late at update. */
    uint32_t refresh_rate_millihz;
    bool running;
    bool shutdown;
} bsp_nixie_status_t;

/* Carrier channels gated by the active-LOW RTC LED on PC13. One cathode mask
 * per anode; anodes also marks gated elements with NIXIE_CHANNEL_FIXED. */
typedef struct {
    uint16_t anodes;
    uint16_t cathodes[NIXIE_MAX_ANODES];
} bsp_nixie_rtc_gate_t;

/* Call after bsp_board_init(). Public operations are serialized in foreground.
 * TIM1 belongs exclusively to this backend. No DMA channels are used. */
bool bsp_nixie_init(void);
const nixie_caps_t* bsp_nixie_caps(void);
/* Copies the plan and optional RTC gate; publishes both at a frame boundary.
 * NULL gate leaves all elements under explicit frame control. During RTC off,
 * gated cathodes are removed; a gated anode with no remaining cathodes stays off.
 * The input pointers are borrowed only for this call. Invalid/BUSY leaves the
 * accepted state unchanged; retry BUSY from foreground. */
nixie_status_t bsp_nixie_present(const nixie_plan_t* plan, const bsp_nixie_rtc_gate_t* rtc_gate);
bool bsp_nixie_set_enabled(bool enabled);
bool bsp_nixie_get_status(bsp_nixie_status_t* status);
/* Latched emergency stop, valid before board init and from fault context.
 * Synchronously stops TIM1 and drives every anode/cathode LOW. */
void bsp_nixie_shutdown(void);
/* Both TIM1 vectors dispatch here with the same NVIC priority. */
void bsp_nixie_timer_irq(void);

#endif /* BSP_NIXIE_H */
