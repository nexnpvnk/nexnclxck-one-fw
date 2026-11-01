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

/* Call after bsp_board_init(). Public operations are serialized in foreground.
 * TIM1 belongs exclusively to this backend. No DMA channels are used. */
bool bsp_nixie_init(void);
const nixie_caps_t* bsp_nixie_caps(void);
/* Copies the plan. While running, publish at the next complete-frame boundary.
 * BUSY leaves both plans unchanged; retry from foreground. */
nixie_status_t bsp_nixie_present(const nixie_plan_t* plan);
bool bsp_nixie_set_enabled(bool enabled);
bool bsp_nixie_get_status(bsp_nixie_status_t* status);
/* Latched emergency stop, valid before board init and from fault context.
 * Synchronously stops TIM1 and drives every anode/cathode LOW. */
void bsp_nixie_shutdown(void);
/* Both TIM1 vectors dispatch here with the same NVIC priority. */
void bsp_nixie_timer_irq(void);

#endif /* BSP_NIXIE_H */
