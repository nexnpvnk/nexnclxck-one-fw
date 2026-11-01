
#ifndef BSP_HV_H
#define BSP_HV_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_board_config.h"

/* Requested ADC rate; Core checks that the configured TIM3 period is exact. */
#define BSP_HV_SAMPLE_RATE_HZ ((uint32_t)BSP_BOARD_HV_ADC_TARGET_RATE_HZ)

#define BSP_HV_PWM_MIN_HZ 20000U
#define BSP_HV_PWM_MAX_HZ 50000U

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_HV_FAULT_NONE = 0,
    BSP_HV_FAULT_INIT,
    BSP_HV_FAULT_ADC,
    BSP_HV_FAULT_OVERVOLTAGE,
    BSP_HV_FAULT_TIMEOUT,
    BSP_HV_FAULT_SHUTDOWN,
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
bool bsp_hv_set_duty(uint16_t permille);
/* Foreground only. Apply at a period boundary, preserving requested duty and enable state. */
bool bsp_hv_set_frequency(uint32_t frequency_hz);

bool bsp_hv_get_status(bsp_hv_status_t* status);

bsp_hv_fault_t bsp_hv_process(void);
void bsp_hv_shutdown(void);

/* Platform dispatch for ADC1_COMP_IRQHandler; not an application callback. */
void bsp_hv_adc_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_HV_H */
