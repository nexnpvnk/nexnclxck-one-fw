
#include "bsp_hv.h"

#include "adc.h"
#include "tim.h"

#include <stddef.h>

/* Board-specific measurement parameters. */
#define BSP_HV_VDDA_MV        3300U
#define BSP_HV_DIVIDER_RATIO  101U
#define BSP_HV_ADC_MAX        4095U

/* Preliminary protection thresholds. */
#define BSP_HV_OVP_VOLTAGE_MV 260000U
#define BSP_HV_ADC_TIMEOUT_MS 10U

#define BSP_HV_RESET_MAX_MV   30000U

/*
 * Commissioning limits.
 *
 * PWM must not be enabled before verifying the
 * gate-driver polarity and MOSFET-off state.
 */
#define BSP_HV_PWM_VERIFIED   0U
#define BSP_HV_DUTY_MAX_PM    100U

#define BSP_HV_OVP_RAW        ((BSP_HV_OVP_VOLTAGE_MV * BSP_HV_ADC_MAX) / (BSP_HV_VDDA_MV * BSP_HV_DIVIDER_RATIO))

static struct {
    volatile uint32_t voltage_mv;
    volatile uint32_t samples;
    volatile uint32_t last_sample_ms;

    volatile uint16_t adc_raw;
    volatile uint16_t duty_permille;

    volatile bool enabled;
    volatile bsp_hv_fault_t fault;

    bool initialized;

    bsp_hv_sample_cb_t callback;
    void* callback_arg;
} hv;

static uint32_t adc_to_voltage(uint16_t raw) {
    return ((uint32_t)raw * BSP_HV_VDDA_MV * BSP_HV_DIVIDER_RATIO + BSP_HV_ADC_MAX / 2U) / BSP_HV_ADC_MAX;
}

static void pwm_disable(void) {
    if (htim17.Instance != TIM17) {
        return;
    }

    /* Disable output before changing the compare value. */
    __HAL_TIM_MOE_DISABLE(&htim17);
    __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 0U);

    /* Transfer the zero compare into the active register. */
    TIM17->EGR = TIM_EGR_UG;

    hv.duty_permille = 0U;
    hv.enabled = false;
}

static void trip(bsp_hv_fault_t reason) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    pwm_disable();

    if (hv.fault == BSP_HV_FAULT_NONE) {
        hv.fault = reason;
    }

    __set_PRIMASK(primask);
}

void bsp_hv_shutdown(void) { trip(BSP_HV_FAULT_SHUTDOWN); }

bool bsp_hv_init(bsp_hv_sample_cb_t callback, void* arg) {
    if (hv.initialized) {
        return true;
    }

    if (hadc.Instance != ADC1 || htim3.Instance != TIM3 || htim17.Instance != TIM17) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    /*
     * Verify the TIM17 configuration expected by
     * the inverting NPN gate driver.
     */
    if ((TIM17->CCER & TIM_CCER_CC1P) == 0U || (TIM17->CR2 & TIM_CR2_OIS1) == 0U
        || (TIM17->BDTR & TIM_BDTR_OSSI) == 0U) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    hv.callback = callback;
    hv.callback_arg = arg;
    hv.fault = BSP_HV_FAULT_NONE;
    hv.enabled = false;

    pwm_disable();

    if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    ADC_AnalogWDGConfTypeDef watchdog = {0};

    watchdog.WatchdogMode = ADC_ANALOGWATCHDOG_SINGLE_REG;
    watchdog.Channel = ADC_CHANNEL_2;
    watchdog.ITMode = ENABLE;
    watchdog.HighThreshold = BSP_HV_OVP_RAW;
    watchdog.LowThreshold = 0U;

    if (HAL_ADC_AnalogWDGConfig(&hadc, &watchdog) != HAL_OK) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    if (HAL_ADC_Start_IT(&hadc) != HAL_OK) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

#if BSP_HV_PWM_VERIFIED
    /*
     * Start with CCR1 = 0.
     * With PWM1 + LOW polarity, PA7 must remain HIGH.
     * Verify this behavior on the actual PCB.
     */
    if (HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1) != HAL_OK) {
        (void)HAL_ADC_Stop_IT(&hadc);
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    pwm_disable();
#endif

    hv.last_sample_ms = HAL_GetTick();
    hv.initialized = true;

    if (HAL_TIM_Base_Start(&htim3) != HAL_OK) {
        hv.initialized = false;
        (void)HAL_ADC_Stop_IT(&hadc);
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    return true;
}

bool bsp_hv_set_enabled(bool enabled) {
    if (!hv.initialized) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (!enabled) {
        pwm_disable();
        __set_PRIMASK(primask);
        return true;
    }

    bool ready = BSP_HV_PWM_VERIFIED && hv.fault == BSP_HV_FAULT_NONE && hv.samples != 0U && hv.adc_raw < BSP_HV_OVP_RAW
                 && (TIM17->CCER & TIM_CCER_CC1E) != 0U
                 && (uint32_t)(HAL_GetTick() - hv.last_sample_ms) <= BSP_HV_ADC_TIMEOUT_MS;

    if (ready) {
        /*
         * CCR1 was set to zero by pwm_disable().
         * Re-enable the timer output without introducing
         * an uncontrolled duty cycle.
         */
        __HAL_TIM_MOE_ENABLE(&htim17);
        hv.enabled = true;
    }

    __set_PRIMASK(primask);
    return ready;
}

bool bsp_hv_set_duty(uint16_t permille) {
    if (!hv.initialized || permille > BSP_HV_DUTY_MAX_PM) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    bool allowed = hv.enabled && !hv.fault;

    if (allowed) {
        uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim17) + 1U;

        uint32_t compare = (period * permille + 500U) / 1000U;

        __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, compare);

        hv.duty_permille = permille;
    }

    __set_PRIMASK(primask);
    return allowed;
}

bool bsp_hv_reset_fault(void) {
    if (!hv.initialized) {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    bool recoverable = hv.fault == BSP_HV_FAULT_OVERVOLTAGE || hv.fault == BSP_HV_FAULT_SHUTDOWN;

    bool safe = !hv.enabled && recoverable && hv.samples != 0U && hv.voltage_mv <= BSP_HV_RESET_MAX_MV
                && (uint32_t)(HAL_GetTick() - hv.last_sample_ms) <= BSP_HV_ADC_TIMEOUT_MS;

    if (safe) {
        pwm_disable();
        hv.fault = BSP_HV_FAULT_NONE;
    }

    __set_PRIMASK(primask);
    return safe;
}

bool bsp_hv_get_status(bsp_hv_status_t* status) {
    if (!hv.initialized || status == NULL) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    status->voltage_mv = hv.voltage_mv;
    status->samples = hv.samples;
    status->adc_raw = hv.adc_raw;
    status->duty_permille = hv.duty_permille;
    status->enabled = hv.enabled;
    status->fault = hv.fault;
    status->enabled = hv.enabled;
    status->pwm_available = BSP_HV_PWM_VERIFIED != 0U;
    status->duty_limit_permille = BSP_HV_DUTY_MAX_PM;

    __set_PRIMASK(primask);
    return true;
}

void bsp_hv_process(void) {
    if (!hv.initialized || !hv.enabled) {
        return;
    }

    if ((uint32_t)(HAL_GetTick() - hv.last_sample_ms) > BSP_HV_ADC_TIMEOUT_MS) {
        /* ADC timeout */
        trip(BSP_HV_FAULT_TIMEOUT);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc_ptr) {
    if (hadc_ptr->Instance != ADC1 || !hv.initialized) {
        return;
    }

    uint16_t raw = (uint16_t)HAL_ADC_GetValue(hadc_ptr);

    if (raw >= BSP_HV_OVP_RAW) {
        /* Analog watchdog / overvoltage */
        trip(BSP_HV_FAULT_OVERVOLTAGE);
        return;
    }

    uint32_t voltage = adc_to_voltage(raw);

    hv.adc_raw = raw;
    hv.voltage_mv = voltage;
    hv.samples++;
    hv.last_sample_ms = HAL_GetTick();

    if (hv.fault == BSP_HV_FAULT_NONE && hv.callback != NULL) {
        hv.callback(voltage, hv.callback_arg);
    }
}

void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc_ptr) {
    if (hadc_ptr->Instance == ADC1) {
        bsp_hv_shutdown();
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc_ptr) {
    if (hadc_ptr->Instance == ADC1) {
        bsp_hv_shutdown();
    }
}
