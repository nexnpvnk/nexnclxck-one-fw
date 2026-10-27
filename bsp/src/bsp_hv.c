
#include "bsp_hv.h"

#include "adc.h"
#include "tim.h"

#include <stddef.h>

#define BSP_HV_VDDA_MV      3300U
#define BSP_HV_DIVIDER_HIGH 1000000U
#define BSP_HV_DIVIDER_LOW  10000U
#define BSP_HV_ADC_MAX      4095U

static struct {
    volatile uint32_t voltage_mv;
    volatile uint32_t samples;
    volatile uint16_t adc_raw;
    volatile bool adc_error;

    bsp_hv_sample_cb_t callback;
    void* callback_arg;

    bool initialized;
} hv;

static uint32_t adc_to_voltage(uint16_t raw) {
    const uint32_t ratio = (BSP_HV_DIVIDER_HIGH + BSP_HV_DIVIDER_LOW) / BSP_HV_DIVIDER_LOW;

    return ((uint32_t)raw * BSP_HV_VDDA_MV * ratio + BSP_HV_ADC_MAX / 2U) / BSP_HV_ADC_MAX;
}

void bsp_hv_shutdown(void) {
    if (htim17.Instance != TIM17) {
        return;
    }

    __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 0U);

    /*
     * Inhibit the timer output.
     * Physical MOSFET-off behavior must be verified
     * against the NPN gate driver and external bias.
     */
    __HAL_TIM_MOE_DISABLE(&htim17);
}

bool bsp_hv_init(bsp_hv_sample_cb_t callback, void* arg) {
    if (hv.initialized) {
        return true;
    }

    if (hadc.Instance != ADC1 || htim3.Instance != TIM3 || htim17.Instance != TIM17) {
        return false;
    }

    bsp_hv_shutdown();

    hv.callback = callback;
    hv.callback_arg = arg;
    hv.adc_error = false;
    hv.samples = 0U;
    hv.adc_raw = 0U;
    hv.voltage_mv = 0U;

    if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK) {
        return false;
    }

    /*
     * Arms ADC for repeated TIM3_TRGO events.
     * No ADC DMA is required.
     */
    if (HAL_ADC_Start_IT(&hadc) != HAL_OK) {
        return false;
    }

    hv.initialized = true;

    if (HAL_TIM_Base_Start(&htim3) != HAL_OK) {
        hv.initialized = false;
        (void)HAL_ADC_Stop_IT(&hadc);
        return false;
    }

    return true;
}

bool bsp_hv_get_status(bsp_hv_status_t* status) {
    if (!hv.initialized || status == NULL) {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    status->voltage_mv = hv.voltage_mv;
    status->samples = hv.samples;
    status->adc_raw = hv.adc_raw;
    status->adc_error = hv.adc_error;

    if (primask == 0U) {
        __enable_irq();
    }

    return true;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc_ptr) {
    if (hadc_ptr->Instance != ADC1 || !hv.initialized) {
        return;
    }

    uint16_t raw = (uint16_t)HAL_ADC_GetValue(hadc_ptr);

    uint32_t voltage = adc_to_voltage(raw);

    hv.adc_raw = raw;
    hv.voltage_mv = voltage;
    hv.samples++;

    if (!hv.adc_error && hv.callback != NULL) {
        hv.callback(voltage, hv.callback_arg);
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc_ptr) {
    if (hadc_ptr->Instance != ADC1) {
        return;
    }

    hv.adc_error = true;
    bsp_hv_shutdown();
}
