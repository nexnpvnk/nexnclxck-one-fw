#ifndef BSP_HV_H
#define BSP_HV_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bsp_hv_sample_cb_t)(uint32_t voltage_mv, void* arg);

typedef struct {
    uint32_t voltage_mv;
    uint32_t samples;
    uint16_t adc_raw;
    bool adc_error;
} bsp_hv_status_t;

bool bsp_hv_init(bsp_hv_sample_cb_t callback, void* arg);
bool bsp_hv_get_status(bsp_hv_status_t* status);

void bsp_hv_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_HV_H */
