#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_BUZZER_FREQ_MIN_HZ     100U
#define BSP_BUZZER_FREQ_MAX_HZ     10000U
#define BSP_BUZZER_FREQ_DEFAULT_HZ 2000U

typedef struct {
    uint32_t frequency_hz;
    bool enabled;
} bsp_buzzer_status_t;

/* Call once after MX_TIM16_Init().
 * PWM starts silent (PA6 low). */
bool bsp_buzzer_init(void);

/* Foreground-only.
 * Changes take effect at a timer update.
 * Frequency is a requested setpoint. */
bool bsp_buzzer_set_frequency(uint32_t frequency_hz);
bool bsp_buzzer_set_enabled(bool enabled);

bool bsp_buzzer_get_status(bsp_buzzer_status_t* status);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BUZZER_H */
