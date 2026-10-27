
#include "app_hv.h"

#include "bsp_hv.h"

#include <stddef.h>

/*
 * ADC:     2000 Hz
 * Control:  100 Hz
 *
 * PI gains are initial values and require hardware tuning.
 */
#define HV_CONTROL_DIVIDER     20U
#define HV_FILTER_SHIFT        4U

#define HV_KP_Q8               64L
#define HV_KI_Q8               8L

#define HV_RAMP_MV_PER_STEP    250U
#define HV_DUTY_RISE_PM        2U

#define HV_REGULATION_BAND_MV  3000U
#define HV_START_TIMEOUT_STEPS 2000U

static struct {
    volatile app_hv_state_t state;
    volatile app_hv_fault_t fault;

    volatile uint16_t target_v;
    volatile uint32_t filtered_mv;
    volatile uint32_t min_mv;
    volatile uint32_t max_mv;

    uint32_t reference_mv;
    uint32_t active_goal_mv;

    int32_t duty_q8;
    int32_t previous_error_dv;

    uint16_t duty_limit_pm;
    uint16_t start_steps;

    uint8_t control_divider;

    bool initialized;
    bool have_sample;
} hv;

static void trip(app_hv_fault_t reason) {
    if (hv.state != APP_HV_FAULT) {
        hv.fault = reason;
    }

    hv.state = APP_HV_FAULT;
    (void)bsp_hv_set_enabled(false);

    hv.duty_q8 = 0;
}

static void control_step(void) {
    uint32_t goal_mv = (uint32_t)hv.target_v * 1000U;

    /* A new target starts a new transition. */
    if (goal_mv != hv.active_goal_mv) {
        hv.active_goal_mv = goal_mv;
        hv.start_steps = 0U;
        hv.state = APP_HV_STARTING;

        /*
         * When lowering the target, discard the accumulated
         * output so the Boost converter does not keep
         * charging the output capacitor.
         */
        if (goal_mv < hv.reference_mv) {
            hv.duty_q8 = 0;
            hv.previous_error_dv = 0;
        }
    }

    /* Ramp upward; lower the reference immediately. */
    if (hv.reference_mv < goal_mv) {
        uint32_t remaining = goal_mv - hv.reference_mv;

        hv.reference_mv += remaining < HV_RAMP_MV_PER_STEP ? remaining : HV_RAMP_MV_PER_STEP;
    } else {
        hv.reference_mv = goal_mv;
    }

    /*
     * Startup timeout is meaningful only before regulation
     * has been reached.
     */
    if (hv.state == APP_HV_STARTING) {
        if (hv.start_steps < HV_START_TIMEOUT_STEPS) {
            hv.start_steps++;
        }

        if (hv.start_steps >= HV_START_TIMEOUT_STEPS && hv.filtered_mv + 10000U < goal_mv) {
            trip(APP_HV_FAULT_NO_REGULATION);
            return;
        }
    }

    /*
     * Incremental PI:
     *
     * u[k] = u[k-1] +
     *        Kp * (e[k] - e[k-1]) +
     *        Ki * e[k]
     *
     * Error is expressed in 0.1 V units.
     * Duty uses Q8 permille.
     */
    int32_t error_dv = ((int32_t)hv.reference_mv - (int32_t)hv.filtered_mv) / 100;

    int32_t delta_q8 = (error_dv - hv.previous_error_dv) * HV_KP_Q8 + error_dv * HV_KI_Q8;

    hv.previous_error_dv = error_dv;

    int32_t duty_q8 = hv.duty_q8 + delta_q8;

    int32_t limit_q8 = (int32_t)hv.duty_limit_pm * 256L;

    /* Saturation also prevents integral windup. */
    if (duty_q8 < 0) {
        duty_q8 = 0;
    } else if (duty_q8 > limit_q8) {
        duty_q8 = limit_q8;
    }

    /* Limit only positive duty changes. */
    int32_t rise_limit_q8 = hv.duty_q8 + (int32_t)HV_DUTY_RISE_PM * 256L;

    if (duty_q8 > rise_limit_q8) {
        duty_q8 = rise_limit_q8;
    }

    if (duty_q8 > limit_q8) {
        duty_q8 = limit_q8;
    }

    uint16_t duty_pm = (uint16_t)((duty_q8 + 128L) / 256L);

    if (!bsp_hv_set_duty(duty_pm)) {
        trip(APP_HV_FAULT_BSP);
        return;
    }

    hv.duty_q8 = duty_q8;

    if (hv.state == APP_HV_STARTING && hv.reference_mv == goal_mv && hv.filtered_mv + HV_REGULATION_BAND_MV >= goal_mv
        && hv.filtered_mv <= goal_mv + HV_REGULATION_BAND_MV) {
        hv.state = APP_HV_RUNNING;
    }
}

/* Called from ADC interrupt context at 2 kHz. */
static void on_sample(uint32_t voltage_mv, void* arg) {
    (void)arg;

    if (!hv.have_sample) {
        hv.filtered_mv = voltage_mv;
        hv.min_mv = voltage_mv;
        hv.max_mv = voltage_mv;
        hv.have_sample = true;
    } else {
        int32_t delta = (int32_t)voltage_mv - (int32_t)hv.filtered_mv;

        hv.filtered_mv = (uint32_t)((int32_t)hv.filtered_mv + delta / (1L << HV_FILTER_SHIFT));

        if (voltage_mv < hv.min_mv) {
            hv.min_mv = voltage_mv;
        }

        if (voltage_mv > hv.max_mv) {
            hv.max_mv = voltage_mv;
        }
    }

    if (hv.state != APP_HV_STARTING && hv.state != APP_HV_RUNNING) {
        return;
    }

    if (++hv.control_divider < HV_CONTROL_DIVIDER) {
        return;
    }

    hv.control_divider = 0U;
    control_step();
}

bool app_hv_init(void) {
    if (hv.initialized) {
        return true;
    }

    hv.state = APP_HV_OFF;
    hv.fault = APP_HV_FAULT_NONE;
    hv.target_v = APP_HV_MIN_VOLTAGE_V;

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
    if (!hv.initialized) {
        return;
    }

    bsp_hv_process();

    /*
     * A hardware fault has priority over application state.
     * This is intentionally outside the control interrupt.
     */
    bsp_hv_status_t status;

    if (bsp_hv_get_status(&status) && status.fault != BSP_HV_FAULT_NONE && hv.state != APP_HV_FAULT) {
        trip(APP_HV_FAULT_BSP);
    }
}

bool app_hv_set_voltage(uint16_t voltage_v) {
    if (!hv.initialized || voltage_v < APP_HV_MIN_VOLTAGE_V || voltage_v > APP_HV_MAX_VOLTAGE_V) {
        return false;
    }

    hv.target_v = voltage_v;
    return true;
}

bool app_hv_set_enabled(bool enabled) {
    if (!hv.initialized) {
        return false;
    }

    if (!enabled) {
        if (hv.state != APP_HV_FAULT) {
            hv.state = APP_HV_OFF;
        }

        (void)bsp_hv_set_enabled(false);

        hv.duty_q8 = 0;
        hv.previous_error_dv = 0;
        hv.control_divider = 0U;

        return true;
    }

    if (!hv.have_sample || hv.state != APP_HV_OFF) {
        return false;
    }

    if (!bsp_hv_set_enabled(true)) {
        return false;
    }

    hv.duty_q8 = 0;
    hv.previous_error_dv = 0;
    hv.control_divider = 0U;
    hv.start_steps = 0U;

    hv.reference_mv = hv.filtered_mv;
    hv.active_goal_mv = (uint32_t)hv.target_v * 1000U;

    hv.state = APP_HV_STARTING;

    return true;
}

bool app_hv_reset_fault(void) {
    if (!hv.initialized || hv.state != APP_HV_FAULT || !bsp_hv_reset_fault()) {
        return false;
    }

    hv.duty_q8 = 0;
    hv.previous_error_dv = 0;
    hv.control_divider = 0U;
    hv.start_steps = 0U;

    hv.fault = APP_HV_FAULT_NONE;
    hv.state = APP_HV_OFF;

    return true;
}

bool app_hv_get_status(app_hv_status_t* status) {
    if (status == NULL || !hv.initialized) {
        return false;
    }

    bsp_hv_status_t hw;

    if (!bsp_hv_get_status(&hw)) {
        return false;
    }

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
