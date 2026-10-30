#ifndef APP_HV_H
#define APP_HV_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_HV_MIN_VOLTAGE_V 100U
#define APP_HV_MAX_VOLTAGE_V 250U
#define APP_HV_DEFAULT_VOLTAGE_V 180U

typedef enum {
    APP_HV_OFF = 0,
    APP_HV_STARTING,
    APP_HV_RUNNING,
    APP_HV_FAULT,
} app_hv_state_t;

typedef enum {
    APP_HV_FAULT_NONE = 0,
    APP_HV_FAULT_INIT,
    APP_HV_FAULT_BSP,
    APP_HV_FAULT_NO_RISE,
    APP_HV_FAULT_NO_REGULATION,
} app_hv_fault_t;

typedef struct {
    uint32_t voltage_mv;
    uint32_t filtered_mv;
    uint32_t samples;
    uint16_t adc_raw;
    uint16_t target_v;
    uint16_t duty_permille;
    uint16_t duty_limit_permille;
    app_hv_state_t state;
    app_hv_fault_t fault;
    uint8_t bsp_fault;
    bool pwm_available;
    bool requested_enabled;
} app_hv_status_t;

/* Successful initialization requests automatic startup at the default voltage. */
bool app_hv_init(void);
void app_hv_process(void);
bool app_hv_set_voltage(uint16_t voltage_v);
/* Enable requests immediate start and automatic retry; disable cancels both. */
bool app_hv_set_enabled(bool enabled);
bool app_hv_get_status(app_hv_status_t* status);

#ifdef __cplusplus
}
#endif

#endif /* APP_HV_H */
