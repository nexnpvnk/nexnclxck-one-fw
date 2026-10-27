#include "app_hv.h"

#include "bsp_hv.h"

#include <stddef.h>

/* Provisional controller gains. Tune using measured startup/load response. */
#define HV_KP_Q8                  20L
#define HV_KI_Q16                 1L
#define HV_RAMP_MV_PER_SAMPLE     100U
#define HV_DUTY_SLEW_PERIOD       4U
#define HV_START_TIMEOUT_SAMPLES  10000U /* 5 s at 2 kHz */
#define HV_NO_RISE_SAMPLES        4000U  /* 2 s at 2 kHz */
#define HV_MIN_RISE_MV            1500U
#define HV_REGULATION_BAND_MV     3000U

static struct {
    volatile app_hv_state_t state;
    volatile app_hv_fault_t fault;
    volatile uint16_t target_v;
    volatile uint32_t filtered_mv;
    volatile uint32_t min_mv;
    volatile uint32_t max_mv;
    uint32_t reference_mv;
    uint32_t start_mv;
    uint32_t ticks;
    int32_t integral_q16;
    uint16_t duty_pm;
    uint16_t duty_limit_pm;
    bool initialized;
    bool have_sample;
} hv;

static void trip(app_hv_fault_t fault) {
    (void)bsp_hv_set_enabled(false);
    if (hv.state != APP_HV_FAULT) {
        hv.fault = fault;
    }
    hv.state = APP_HV_FAULT;
}

/* Executed in ADC interrupt context; no printing, blocking or allocation. */
static void on_sample(uint32_t voltage_mv, void* arg) {
    (void)arg;

    if (!hv.have_sample) {
        hv.filtered_mv = voltage_mv;
        hv.min_mv = voltage_mv;
        hv.max_mv = voltage_mv;
        hv.have_sample = true;
    } else {
        int32_t delta = (int32_t)voltage_mv - (int32_t)hv.filtered_mv;
        hv.filtered_mv = (uint32_t)((int32_t)hv.filtered_mv + delta / 4);
        if (voltage_mv < hv.min_mv) hv.min_mv = voltage_mv;
        if (voltage_mv > hv.max_mv) hv.max_mv = voltage_mv;
    }

    if (hv.state != APP_HV_STARTING && hv.state != APP_HV_RUNNING) {
        return;
    }

    ++hv.ticks;
    uint32_t goal_mv = (uint32_t)hv.target_v * 1000U;
    if (hv.reference_mv < goal_mv) {
        uint32_t next = hv.reference_mv + HV_RAMP_MV_PER_SAMPLE;
        hv.reference_mv = next < goal_mv ? next : goal_mv;
    } else if (hv.reference_mv > goal_mv) {
        hv.reference_mv = goal_mv;
        hv.integral_q16 = 0;
    }

    if (hv.ticks >= HV_NO_RISE_SAMPLES &&
        hv.filtered_mv < hv.start_mv + HV_MIN_RISE_MV) {
        trip(APP_HV_FAULT_NO_RISE);
        return;
    }
    if (hv.ticks >= HV_START_TIMEOUT_SAMPLES &&
        hv.filtered_mv + 10000U < goal_mv) {
        trip(APP_HV_FAULT_NO_REGULATION);
        return;
    }

    int32_t error_dv = ((int32_t)hv.reference_mv - (int32_t)hv.filtered_mv) / 100;
    if (error_dv < -20) {
        /* Boost cannot sink charge: allow the output to discharge naturally. */
        hv.integral_q16 = 0;
        if (!bsp_hv_set_duty(0U)) trip(APP_HV_FAULT_BSP);
        hv.duty_pm = 0U;
        return;
    }

    int32_t p_pm = (error_dv * HV_KP_Q8) / 256;
    int32_t next_i = hv.integral_q16 + error_dv * HV_KI_Q16;
    int32_t limit_q16 = (int32_t)hv.duty_limit_pm * 65536L;
    if (next_i < 0) next_i = 0;
    if (next_i > limit_q16) next_i = limit_q16;

    int32_t tentative = p_pm + next_i / 65536L;
    if (!((tentative >= hv.duty_limit_pm && error_dv > 0) ||
          (tentative <= 0 && error_dv < 0))) {
        hv.integral_q16 = next_i;
    }
    int32_t duty = p_pm + hv.integral_q16 / 65536L;
    if (duty < 0) duty = 0;
    if (duty > hv.duty_limit_pm) duty = hv.duty_limit_pm;

    /* Limit positive duty changes to 1 permille per four ADC samples. */
    if (duty > hv.duty_pm && (hv.ticks % HV_DUTY_SLEW_PERIOD) != 0U) {
        duty = hv.duty_pm;
    } else if (duty > (int32_t)hv.duty_pm + 1) {
        duty = hv.duty_pm + 1;
    }
    if (!bsp_hv_set_duty((uint16_t)duty)) {
        trip(APP_HV_FAULT_BSP);
        return;
    }
    hv.duty_pm = (uint16_t)duty;

    if (hv.reference_mv == goal_mv &&
        hv.filtered_mv + HV_REGULATION_BAND_MV >= goal_mv &&
        hv.filtered_mv <= goal_mv + HV_REGULATION_BAND_MV) {
        hv.state = APP_HV_RUNNING;
    }
}

bool app_hv_init(void) {
    if (hv.initialized) {
        return true;
    }
    hv.state = APP_HV_OFF;
    hv.target_v = APP_HV_MIN_VOLTAGE_V;
    hv.fault = APP_HV_FAULT_NONE;
    if (!bsp_hv_init(on_sample, NULL)) {
        hv.state = APP_HV_FAULT;
        hv.fault = APP_HV_FAULT_INIT;
        return false;
    }
    bsp_hv_status_t status;
    if (!bsp_hv_get_status(&status)) {
        hv.state = APP_HV_FAULT;
        hv.fault = APP_HV_FAULT_INIT;
        return false;
    }
    hv.duty_limit_pm = status.duty_limit_permille;
    hv.initialized = true;
    return true;
}

void app_hv_process(void) {
    if (!hv.initialized) return;
    bsp_hv_process();
    bsp_hv_status_t status;
    if (bsp_hv_get_status(&status) && status.fault != BSP_HV_FAULT_NONE &&
        hv.state != APP_HV_FAULT) {
        trip(APP_HV_FAULT_BSP);
    }
}

bool app_hv_set_voltage(uint16_t voltage_v) {
    if (!hv.initialized || voltage_v < APP_HV_MIN_VOLTAGE_V ||
        voltage_v > APP_HV_MAX_VOLTAGE_V) return false;
    hv.target_v = voltage_v; /* 16-bit aligned stores are atomic on Cortex-M0. */
    return true;
}

bool app_hv_set_enabled(bool enabled) {
    if (!hv.initialized) return false;
    if (!enabled) {
        /* Publish OFF first: the ADC ISR must not update duty during stop. */
        if (hv.state != APP_HV_FAULT) hv.state = APP_HV_OFF;
        (void)bsp_hv_set_enabled(false);
        hv.duty_pm = 0U;
        hv.integral_q16 = 0;
        return true;
    }
    if (!hv.have_sample || hv.state != APP_HV_OFF) return false;
    if (!bsp_hv_set_enabled(true)) return false;
    hv.integral_q16 = 0;
    hv.ticks = 0U;
    hv.duty_pm = 0U;
    hv.start_mv = hv.filtered_mv;
    hv.reference_mv = hv.filtered_mv;
    hv.state = APP_HV_STARTING;
    return true;
}

bool app_hv_reset_fault(void) {
    if (!hv.initialized || hv.state != APP_HV_FAULT ||
        !bsp_hv_reset_fault()) return false;
    hv.integral_q16 = 0;
    hv.duty_pm = 0U;
    hv.fault = APP_HV_FAULT_NONE;
    hv.state = APP_HV_OFF;
    return true;
}

bool app_hv_get_status(app_hv_status_t* status) {
    if (status == NULL || !hv.initialized) return false;
    bsp_hv_status_t hw;
    if (!bsp_hv_get_status(&hw)) return false;
    status->voltage_mv = hw.voltage_mv;
    status->filtered_mv = hv.filtered_mv;
    status->min_mv = hv.min_mv;
    status->max_mv = hv.max_mv;
    status->samples = hw.samples;
    status->adc_raw = hw.adc_raw;
    status->target_v = hv.target_v;
    status->duty_permille = hw.duty_permille;
    status->duty_limit_permille = hw.duty_limit_permille;
    status->state = hv.state;
    status->fault = hv.fault;
    status->bsp_fault = (uint8_t)hw.fault;
    status->pwm_available = hw.pwm_available;
    return true;
}
