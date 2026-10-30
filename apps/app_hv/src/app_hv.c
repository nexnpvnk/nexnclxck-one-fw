
#include "app_hv.h"

#include "bsp_hv.h"
#include "bsp_time.h"

#include <stddef.h>

/*
 * ADC:     2000 Hz
 * Control: 1000 Hz
 *
 * Duty uses Q16 permille; error uses 0.1 V units.
 * Kp = 2.5 permille/V; Ki ~= 4.88 permille/(V*s) at 1 kHz.
 * These gains require validation on the board, including no-load startup.
 */
#define HV_ADC_RATE_HZ         2000U
#define HV_CONTROL_DIVIDER     2U
#define HV_CONTROL_RATE_HZ     (HV_ADC_RATE_HZ / HV_CONTROL_DIVIDER)
#define HV_FILTER_SHIFT        2U

#define HV_KP_Q16              16384L
#define HV_KI_Q16              (32000L / (int32_t)HV_CONTROL_RATE_HZ)

#define HV_REGULATION_BAND_MV  3000U
#define HV_SETTLE_STEPS        (HV_CONTROL_RATE_HZ / 20U) /* 50 ms within the band. */
#define HV_START_TIMEOUT_STEPS (20U * HV_CONTROL_RATE_HZ)
#define HV_RETRY_DELAY_MS      1000U

static struct {
    volatile app_hv_state_t state;
    volatile app_hv_fault_t fault;

    volatile uint16_t target_v;
    volatile uint32_t filtered_mv;

    uint32_t active_goal_mv;

    int32_t integral_q16;

    uint16_t duty_limit_pm;
    uint16_t start_steps;
    uint16_t settle_steps;

    uint8_t control_divider;

    volatile uint32_t last_attempt_ms;
    bool requested_enabled;
    bool initialized;
    volatile bool have_sample;
} hv;

static void trip(app_hv_fault_t reason) {
    if (hv.state != APP_HV_FAULT) {
        hv.fault = reason;
        hv.last_attempt_ms = bsp_time_ms();
    }

    hv.state = APP_HV_FAULT;
    (void)bsp_hv_set_enabled(false);

    hv.integral_q16 = 0;
}

static void control_step(void) {
    uint32_t goal_mv = (uint32_t)hv.target_v * 1000U;

    /* A new target starts a transition, preserving the operating integral.
     * Negative error reduces P immediately and unwinds I without a restart at zero. */
    if (goal_mv != hv.active_goal_mv) {
        hv.active_goal_mv = goal_mv;
        hv.start_steps = 0U;
        hv.settle_steps = 0U;
        hv.state = APP_HV_STARTING;
    }

    /*
     * Allow 20 seconds to acquire or regain regulation.
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
     * Positional PI, using the full target immediately (no soft start):
     * u[k] = Kp * e[k] + I[k], I[k] = I[k-1] + Ki * e[k].
     *
     * Keep the integral separate from the applied duty. There is no duty
     * slew limiter to discard one half of the proportional response.
     */
    int32_t error_dv = ((int32_t)goal_mv - (int32_t)hv.filtered_mv) / 100;
    int32_t proportional_q16 = error_dv * HV_KP_Q16;
    int32_t limit_q16 = (int32_t)hv.duty_limit_pm * 65536L;
    int32_t integral_q16 = hv.integral_q16 + error_dv * HV_KI_Q16;

    /* The steady-state duty is nonnegative and cannot exceed the BSP limit. */
    if (integral_q16 < 0) {
        integral_q16 = 0;
    } else if (integral_q16 > limit_q16) {
        integral_q16 = limit_q16;
    }

    int32_t duty_q16 = proportional_q16 + integral_q16;

    /* Freeze integration only when the error drives the output further
     * into saturation. Integration in the recovery direction is allowed. */
    if ((duty_q16 > limit_q16 && error_dv > 0) || (duty_q16 < 0 && error_dv < 0)) {
        duty_q16 = proportional_q16 + hv.integral_q16;
    } else {
        hv.integral_q16 = integral_q16;
    }

    if (duty_q16 < 0) {
        duty_q16 = 0;
    } else if (duty_q16 > limit_q16) {
        duty_q16 = limit_q16;
    }

    uint16_t duty_pm = (uint16_t)((duty_q16 + 32768L) / 65536L);

    if (!bsp_hv_set_duty(duty_pm)) {
        trip(APP_HV_FAULT_BSP);
        return;
    }

    if (hv.filtered_mv + HV_REGULATION_BAND_MV >= goal_mv && hv.filtered_mv <= goal_mv + HV_REGULATION_BAND_MV) {
        if (hv.state == APP_HV_STARTING) {
            if (++hv.settle_steps >= HV_SETTLE_STEPS) {
                hv.state = APP_HV_RUNNING;
                hv.start_steps = 0U;
            }
        }
    } else {
        hv.settle_steps = 0U;
        if (hv.state == APP_HV_RUNNING) {
            hv.state = APP_HV_STARTING;
            hv.start_steps = 0U;
        }
    }
}

/* Called from ADC interrupt context at 2 kHz. */
static void on_sample(uint32_t voltage_mv, bsp_hv_fault_t fault, bool valid, void* arg) {
    (void)arg;

    if (valid) {
        if (!hv.have_sample) {
            hv.filtered_mv = voltage_mv;
            hv.have_sample = true;
        } else {
            int32_t delta = (int32_t)voltage_mv - (int32_t)hv.filtered_mv;
            hv.filtered_mv = (uint32_t)((int32_t)hv.filtered_mv + delta / (1L << HV_FILTER_SHIFT));
        }
    }

    if (hv.state != APP_HV_STARTING && hv.state != APP_HV_RUNNING) {
        return;
    }

    if (fault != BSP_HV_FAULT_NONE) {
        trip(APP_HV_FAULT_BSP);
        return;
    }

    if (!valid) {
        hv.control_divider = 0U;
        hv.settle_steps = 0U;
        return;
    }

    if (++hv.control_divider < HV_CONTROL_DIVIDER) {
        return;
    }

    hv.control_divider = 0U;

    control_step();
}

/* Foreground only. OFF/FAULT keeps ADC control inactive until all PI state is ready. */
static bool start(void) {
    if (!hv.have_sample) {
        return false;
    }

    bsp_hv_status_t status;
    if (!bsp_hv_get_status(&status)) {
        return false;
    }
    if (status.enabled && status.fault == BSP_HV_FAULT_NONE
        && (hv.state == APP_HV_STARTING || hv.state == APP_HV_RUNNING)) {
        return true;
    }

    /* A BSP trip can precede foreground fault processing. */
    if (hv.state == APP_HV_STARTING || hv.state == APP_HV_RUNNING) {
        trip(APP_HV_FAULT_BSP);
    }

    if (!bsp_hv_set_enabled(true)) {
        return false;
    }

    hv.integral_q16 = 0;
    hv.control_divider = 0U;
    hv.start_steps = 0U;
    hv.settle_steps = 0U;

    hv.active_goal_mv = (uint32_t)hv.target_v * 1000U;

    hv.fault = APP_HV_FAULT_NONE;
    hv.state = APP_HV_STARTING;

    return true;
}

bool app_hv_init(void) {
    if (hv.initialized) {
        return true;
    }

    hv.state = APP_HV_OFF;
    hv.fault = APP_HV_FAULT_NONE;
    hv.target_v = APP_HV_DEFAULT_VOLTAGE_V;

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
    hv.requested_enabled = true;
    /* The first foreground pass with an ADC sample may start immediately. */
    hv.last_attempt_ms = bsp_time_ms() - HV_RETRY_DELAY_MS;
    hv.initialized = true;

    return true;
}

void app_hv_process(void) {
    if (!hv.initialized) {
        return;
    }

    bsp_hv_fault_t fault = bsp_hv_process();
    if (fault != BSP_HV_FAULT_NONE && hv.state != APP_HV_FAULT) {
        trip(APP_HV_FAULT_BSP);
    }

    if (!hv.requested_enabled || !hv.have_sample || (hv.state != APP_HV_OFF && hv.state != APP_HV_FAULT)) {
        return;
    }

    /* Never automatically undo an explicit shutdown or retry failed setup. */
    if (fault != BSP_HV_FAULT_NONE && fault != BSP_HV_FAULT_OVERVOLTAGE && fault != BSP_HV_FAULT_TIMEOUT) {
        return;
    }

    uint32_t now = bsp_time_ms();
    if ((uint32_t)(now - hv.last_attempt_ms) >= HV_RETRY_DELAY_MS) {
        hv.last_attempt_ms = now;
        (void)start(); /* BSP checks fresh ADC, OVP and PWM readiness. */
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

    hv.requested_enabled = enabled;
    if (!enabled) {
        if (hv.state != APP_HV_FAULT) {
            hv.state = APP_HV_OFF;
        }

        (void)bsp_hv_set_enabled(false);

        hv.integral_q16 = 0;
        hv.control_divider = 0U;
        hv.settle_steps = 0U;

        return true;
    }

    hv.last_attempt_ms = bsp_time_ms();
    (void)start(); /* If not ready yet, the accepted request remains queued. */
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

    status->samples = hw.samples;
    status->adc_raw = hw.adc_raw;

    status->target_v = hv.target_v;
    status->duty_permille = hw.duty_permille;
    status->duty_limit_permille = hw.duty_limit_permille;

    status->state = hv.state;
    status->fault = hv.fault;
    status->bsp_fault = (uint8_t)hw.fault;
    status->pwm_available = hw.pwm_available;
    status->requested_enabled = hv.requested_enabled;

    return true;
}
