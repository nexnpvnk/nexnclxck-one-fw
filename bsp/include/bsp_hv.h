
#ifndef BSP_HV_H
#define BSP_HV_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bsp_hv_sample_cb_t)(uint32_t voltage_mv, void* arg);

typedef enum {
    BSP_HV_FAULT_NONE = 0,
    BSP_HV_FAULT_INIT,
    BSP_HV_FAULT_ADC,
    BSP_HV_FAULT_OVERVOLTAGE,
    BSP_HV_FAULT_TIMEOUT,
    BSP_HV_FAULT_SHUTDOWN,
} bsp_hv_fault_t;

typedef struct {
    uint32_t voltage_mv;
    uint32_t samples;
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
bool bsp_hv_reset_fault(void);

bool bsp_hv_get_status(bsp_hv_status_t* status);

void bsp_hv_process(void);
void bsp_hv_shutdown(void);

/* Platform dispatch for ADC1_COMP_IRQHandler; not an application callback. */
void bsp_hv_adc_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_HV_H */
