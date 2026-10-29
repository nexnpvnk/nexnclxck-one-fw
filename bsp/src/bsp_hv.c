
#include "bsp_hv.h"

#include "adc.h"
#include "bsp_time.h"
#include "tim.h"

#include <stddef.h>

#define BSP_HV_VDDA_MV             3300U
#define BSP_HV_DIVIDER_RATIO       101U
#define BSP_HV_ADC_MAX             4095U

#define BSP_HV_OVP_VOLTAGE_MV      260000U

#define BSP_HV_ADC_TIMEOUT_MS      10U
#define BSP_HV_ADC_INIT_TIMEOUT_MS 100U

/*
 * Hardware duty limit.
 * This does not guarantee safe inductor/MOSFET current.
 */
#define BSP_HV_DUTY_MAX_PM         980U

#define BSP_HV_OVP_RAW             ((BSP_HV_OVP_VOLTAGE_MV * BSP_HV_ADC_MAX) / (BSP_HV_VDDA_MV * BSP_HV_DIVIDER_RATIO))

static struct {
    volatile uint32_t voltage_mv;
    volatile uint32_t samples;
    volatile uint32_t last_sample_ms;

    volatile uint16_t adc_raw;
    volatile uint16_t duty_permille;
    volatile uint16_t trip_adc_raw;
    volatile uint32_t trip_adc_isr;
    volatile uint32_t trip_adc_tr;

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
    /* Fault handlers may call this before TIM17 has been initialized. */
    if ((RCC->APB2ENR & RCC_APB2ENR_TIM17EN) != 0U) {
        /* Inhibit the output before resetting CCR1 and loading the preload. */
        TIM17->BDTR &= ~TIM_BDTR_MOE;
        TIM17->CCR1 = 0U;
        TIM17->EGR = TIM_EGR_UG;
        __DSB();
    }

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

static bool adc_wait(volatile uint32_t* reg, uint32_t mask, uint32_t value) {
    uint32_t start = bsp_time_ms();
    while ((*reg & mask) != value) {
        if ((uint32_t)(bsp_time_ms() - start) >= BSP_HV_ADC_INIT_TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

bool bsp_hv_init(bsp_hv_sample_cb_t callback, void* arg) {
    if (hv.initialized) {
        return true;
    }

    /* Check only CKMODE in CFGR2: reserved bits can read back nonzero. */
    if ((RCC->APB2ENR & (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM17EN)) != (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM17EN)
        || (RCC->APB1ENR & RCC_APB1ENR_TIM3EN) == 0U || (RCC->CR2 & RCC_CR2_HSI14RDY) == 0U
        || SystemCoreClock != 48000000U || (RCC->CFGR & RCC_CFGR_PPRE) != 0U || TIM3->PSC != 47U || TIM3->ARR != 499U
        || (TIM3->CR2 & TIM_CR2_MMS) != TIM_CR2_MMS_1 || (TIM3->CR1 & TIM_CR1_CEN) != 0U || TIM17->PSC != 0U
        || TIM17->ARR != 2399U || (ADC1->CFGR2 & ADC_CFGR2_CKMODE) != 0U || ADC1->CHSELR != ADC_CHSELR_CHSEL2
        || ADC1->SMPR != ADC_SMPR_SMP
        || ADC1->CFGR1 != (ADC_CFGR1_EXTSEL_0 | ADC_CFGR1_EXTSEL_1 | ADC_CFGR1_EXTEN_0 | ADC_CFGR1_OVRMOD)
        || ADC1->CR != 0U) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    /*
     * The board has an inverting NPN gate driver.
     * TIM17 must use inverted polarity and HIGH idle state.
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
    hv.samples = 0U;

    pwm_disable();

    /* Calibration requires ADEN=0 and DMAEN=0. TIM3 is still stopped. */
    ADC1->IER = 0U;
    ADC1->CR |= ADC_CR_ADCAL;
    if (!adc_wait(&ADC1->CR, ADC_CR_ADCAL, 0U)) {
        trip(BSP_HV_FAULT_INIT);
        return false;
    }

    ADC1->TR = BSP_HV_OVP_RAW << ADC_TR1_HT1_Pos;
    ADC1->CFGR1 |= ADC_CFGR1_AWDSGL | ADC_CFGR1_AWDEN | ADC_CFGR1_AWDCH_1;

    ADC1->ISR = ADC_ISR_ADRDY;
    uint32_t start = bsp_time_ms();
    /* ES0202: calibration can clear ADEN for four ADC clocks. Reassert until ready. */
    while ((ADC1->ISR & ADC_ISR_ADRDY) == 0U) {
        ADC1->CR |= ADC_CR_ADEN;
        if ((uint32_t)(bsp_time_ms() - start) >= BSP_HV_ADC_INIT_TIMEOUT_MS) {
            trip(BSP_HV_FAULT_INIT);
            return false;
        }
    }

    /*
     * Start PWM with zero duty.
     * PWM1 + LOW polarity: CCR1 = 0 means MOSFET OFF.
     */
    TIM17->CCER |= TIM_CCER_CC1E;
    TIM17->CR1 |= TIM_CR1_CEN; /* MOE stays clear until bsp_hv_set_enabled(true). */

    pwm_disable();

    hv.last_sample_ms = bsp_time_ms();
    hv.initialized = true;

    ADC1->ISR = ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR | ADC_ISR_AWD;
    NVIC_ClearPendingIRQ(ADC1_COMP_IRQn);
    ADC1->IER = ADC_IER_EOCIE | ADC_IER_OVRIE | ADC_IER_AWDIE;
    __DMB();
    ADC1->CR |= ADC_CR_ADSTART; /* Arm the external trigger, then start its source. */
    TIM3->CR1 |= TIM_CR1_CEN;

    return true;
}

bool bsp_hv_set_enabled(bool enabled) {
    if (!hv.initialized) {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (!enabled) {
        pwm_disable();
        __set_PRIMASK(primask);
        return true;
    }

    /* Enable also retries runtime faults. Initialization/ADC
     * failures require repair; a stale or overvoltage sample cannot enable PWM. */
    bool recoverable = hv.fault == BSP_HV_FAULT_NONE || hv.fault == BSP_HV_FAULT_OVERVOLTAGE
                       || hv.fault == BSP_HV_FAULT_TIMEOUT || hv.fault == BSP_HV_FAULT_SHUTDOWN;
    bool ready = recoverable && hv.samples != 0U && hv.adc_raw < BSP_HV_OVP_RAW
                 && (TIM17->CCER & TIM_CCER_CC1E) != 0U && (TIM17->CR1 & TIM_CR1_CEN) != 0U
                 && (ADC1->ISR & ADC_ISR_AWD) == 0U
                 && (uint32_t)(bsp_time_ms() - hv.last_sample_ms) <= BSP_HV_ADC_TIMEOUT_MS;

    if (ready) {
        if (!hv.enabled) {
            pwm_disable();
            hv.fault = BSP_HV_FAULT_NONE;
        }
        TIM17->BDTR |= TIM_BDTR_MOE;
        hv.enabled = true;
    }

    __set_PRIMASK(primask);
    return ready;
}

bool bsp_hv_set_duty(uint16_t permille) {
    if (!hv.initialized || permille > BSP_HV_DUTY_MAX_PM) {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    bool allowed = hv.enabled && hv.fault == BSP_HV_FAULT_NONE;

    if (allowed) {
        uint32_t period = TIM17->ARR + 1U;

        uint32_t compare = (period * permille + 500U) / 1000U;

        TIM17->CCR1 = compare;

        hv.duty_permille = permille;
    }

    __set_PRIMASK(primask);
    return allowed;
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

    status->duty_permille = hv.duty_permille;
    status->duty_limit_permille = BSP_HV_DUTY_MAX_PM;
    status->trip_adc_raw = hv.trip_adc_raw;
    status->trip_adc_isr = hv.trip_adc_isr;
    status->trip_adc_tr = hv.trip_adc_tr;

    status->enabled = hv.enabled;
    status->fault = hv.fault;

    status->pwm_available = (TIM17->CCER & TIM_CCER_CC1E) != 0U && (TIM17->CR1 & TIM_CR1_CEN) != 0U;

    __set_PRIMASK(primask);
    return true;
}

bsp_hv_fault_t bsp_hv_process(void) {
    if (hv.initialized && hv.enabled) {
        /* Read the sample timestamp before time, so an ADC IRQ cannot make
         * the subtraction wrap by publishing a newer timestamp in between. */
        uint32_t last_sample_ms = hv.last_sample_ms;
        uint32_t now = bsp_time_ms();
        if ((uint32_t)(now - last_sample_ms) > BSP_HV_ADC_TIMEOUT_MS && last_sample_ms == hv.last_sample_ms) {
            trip(BSP_HV_FAULT_TIMEOUT);
        }
    }
    return hv.fault;
}

void bsp_hv_adc_irq(void) {
    uint32_t flags = ADC1->ISR;
    uint32_t pending = flags & ADC1->IER;
    bool have_sample = (pending & ADC_ISR_EOC) != 0U;
    uint16_t raw = have_sample ? (uint16_t)ADC1->DR : hv.adc_raw;

    /* Disable PWM before publishing the sample or calling the application.
     * Keep the first OVP snapshot even after later samples have decayed. */
    if ((pending & ADC_ISR_AWD) != 0U || (have_sample && raw >= BSP_HV_OVP_RAW)) {
        if (hv.fault == BSP_HV_FAULT_NONE) {
            hv.trip_adc_raw = raw;
            hv.trip_adc_isr = flags;
            hv.trip_adc_tr = ADC1->TR;
        }
        trip(BSP_HV_FAULT_OVERVOLTAGE);
    }

    if (!have_sample) {
        ADC1->ISR = pending & (ADC_ISR_AWD | ADC_ISR_OVR);
        return;
    }

    /* Preserve overwrite-on-overrun semantics: consume the latest sample.
     * Clear the snapshot flags before running the application callback. */
    ADC1->ISR = flags & (ADC_ISR_EOS | ADC_ISR_OVR | ADC_ISR_AWD);
    if (!hv.initialized) {
        return;
    }

    uint32_t voltage = adc_to_voltage(raw);

    hv.adc_raw = raw;
    hv.voltage_mv = voltage;
    hv.samples++;
    hv.last_sample_ms = bsp_time_ms();

    /* Publish fault with the measurement so control needs no status snapshot.
     * Measurements keep updating during a fault; duty writes remain blocked. */
    if (hv.callback != NULL) {
        hv.callback(voltage, hv.fault, hv.callback_arg);
    }
}
