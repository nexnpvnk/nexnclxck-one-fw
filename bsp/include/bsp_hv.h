
#ifndef BSP_HV_H
#define BSP_HV_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_board_config.h"

/* Nominal ADC/control rate. The trigger period is rounded to whole PWM cycles. */
#define BSP_HV_SAMPLE_RATE_HZ ((uint32_t)BSP_BOARD_HV_ADC_TARGET_RATE_HZ)

#define BSP_HV_PWM_MIN_HZ 20000U
#define BSP_HV_PWM_MAX_HZ 50000U
/* Q16 fraction of a complete PWM period: 65536 means 100%. */
#define BSP_HV_DUTY_SCALE     65536UL

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_HV_FAULT_NONE = 0,
    BSP_HV_FAULT_INIT = 1,
    BSP_HV_FAULT_OVERVOLTAGE = 3,
    BSP_HV_FAULT_TIMEOUT = 4,
    BSP_HV_FAULT_SHUTDOWN = 5,
} bsp_hv_fault_t;

/* Invalid samples update raw telemetry but must not drive the PI/filter. */
typedef void (*bsp_hv_sample_cb_t)(uint32_t voltage_mv, bsp_hv_fault_t fault, bool valid, void* arg);

typedef struct {
    uint32_t voltage_mv;
    uint32_t samples;
    uint32_t pwm_frequency_hz;
    uint16_t adc_raw;
    uint16_t duty_permille;
    uint16_t duty_limit_permille;

    bsp_hv_fault_t fault;

    bool enabled;
    bool pwm_available;
} bsp_hv_status_t;

bool bsp_hv_init(bsp_hv_sample_cb_t callback, void* arg);

bool bsp_hv_set_enabled(bool enabled);
/* Round once, directly to the current timer tick; values above the duty limit
 * are rejected. Telemetry still reports duty in integer permille. */
bool bsp_hv_set_duty_q16(uint32_t duty_q16);
/* Foreground only. Apply at a period boundary, preserving requested duty and enable state. */
bool bsp_hv_set_frequency(uint32_t frequency_hz);

bool bsp_hv_get_status(bsp_hv_status_t* status);

bsp_hv_fault_t bsp_hv_process(void);
void bsp_hv_shutdown(void);

/* Platform dispatch for ADC1_COMP_IRQHandler; not an application callback. */
void bsp_hv_adc_irq(void);
/* One update interrupt after changing/resetting PWM phase, not per PWM cycle. */
void bsp_hv_pwm_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_HV_H */
