
#include "bsp_hv.h"

#include "bsp_time.h"
#include "bsp_hw.h"
#include "bsp_hv_diag.h"
#ifdef HV_DIAGNOSTICS
#include "bsp_pins.h"
#endif

#include <stddef.h>

#define BSP_HV_VDDA_MV             3300U
#define BSP_HV_DIVIDER_RATIO       101U
#define BSP_HV_ADC_MAX             4095U

#define BSP_HV_OVP_VOLTAGE_MV      260000U
#define BSP_HV_OVP_CONFIRM_SAMPLES 3U

#define BSP_HV_ADC_TIMEOUT_MS      10U
#define BSP_HV_ADC_INIT_TIMEOUT_MS 100U
#define BSP_HV_ADC_SAMPLE_TIME     (ADC_SMPR_SMP_2 | ADC_SMPR_SMP_0) /* 55.5 clocks: 3.96 us. */
#define BSP_HV_SAMPLE_OFFSET_TICKS (2U * (PLATFORM_HCLK_HZ / 1000000U))

/*
 * Hardware duty limit.
 * This does not guarantee safe inductor/MOSFET current.
 */
#define BSP_HV_DUTY_MAX_PM         980U
#define BSP_HV_DUTY_MAX_Q16        ((BSP_HV_DUTY_MAX_PM * BSP_HV_DUTY_SCALE) / 1000U)

#define BSP_HV_PWM_TICK_HZ ((uint32_t)PLATFORM_HCLK_HZ) /* TIM17 PSC=0. */

_Static_assert(BSP_BOARD_HV_PWM_RATE_HZ >= BSP_HV_PWM_MIN_HZ
                   && BSP_BOARD_HV_PWM_RATE_HZ <= BSP_HV_PWM_MAX_HZ,
               "The default HV PWM frequency must be within the runtime range");
_Static_assert(BSP_HV_PWM_TICK_HZ / BSP_HV_PWM_MAX_HZ >= 2U
                   && (BSP_HV_PWM_TICK_HZ + BSP_HV_PWM_MIN_HZ / 2U) / BSP_HV_PWM_MIN_HZ <= (uint32_t)UINT16_MAX + 1U,
               "The runtime HV PWM periods must fit the 16-bit timer");
_Static_assert(((uint64_t)(BSP_HV_PWM_TICK_HZ + BSP_HV_PWM_MIN_HZ / 2U) / BSP_HV_PWM_MIN_HZ) * BSP_HV_DUTY_SCALE
                       + BSP_HV_DUTY_SCALE / 2U
                   <= UINT32_MAX,
               "Q16 duty conversion must fit 32 bits at the longest PWM period");
_Static_assert(PLATFORM_HCLK_HZ / BSP_HV_SAMPLE_RATE_HZ > PLATFORM_HCLK_HZ / BSP_HV_PWM_MIN_HZ
                   && PLATFORM_HCLK_HZ / BSP_HV_SAMPLE_RATE_HZ + PLATFORM_HCLK_HZ / BSP_HV_PWM_MIN_HZ / 2U
                          <= (uint32_t)UINT16_MAX + 1U,
               "The PWM-aligned sample period must fit TIM3 throughout the runtime range");
_Static_assert(BSP_HV_SAMPLE_OFFSET_TICKS < PLATFORM_HCLK_HZ / BSP_HV_PWM_MAX_HZ,
               "The ADC sample offset must fit the shortest PWM period");

#define BSP_HV_OVP_RAW             ((BSP_HV_OVP_VOLTAGE_MV * BSP_HV_ADC_MAX) / (BSP_HV_VDDA_MV * BSP_HV_DIVIDER_RATIO))

static struct {
    volatile uint32_t voltage_mv;
    volatile uint32_t samples;
    volatile uint32_t last_sample_ms;

    volatile uint16_t adc_raw;
    volatile uint32_t duty_q16;
    uint8_t ovp_samples;
    volatile bool ovp_suspended;

    volatile bool enabled;
    volatile bsp_hv_fault_t fault;

    bool initialized;

    bsp_hv_sample_cb_t callback;
    void* callback_arg;
} hv;

#ifdef HV_DIAGNOSTICS
_Static_assert(sizeof(bsp_hv_trace_t) == 16U && offsetof(bsp_hv_diag_t, records) == 72U,
               "The SWD capture layout must match debug/hv_capture.py");
volatile bsp_hv_diag_t bsp_hv_diag = {.capture = 1U, .decimation = 40U};

static void trace_sample(uint16_t raw, uint32_t flags, bool valid, uint16_t phase, uint16_t started) {
    if (bsp_hv_diag.capture == 1U) {
        bsp_hv_diag.count = 0U;
        bsp_hv_diag.samples = 0U;
        bsp_hv_diag.rejected = 0U;
        bsp_hv_diag.ovp_holds = 0U;
        bsp_hv_diag.overruns = 0U;
        bsp_hv_diag.divider = 0U;
        bsp_hv_diag.interval_flags = 0U;
        bsp_hv_diag.irq_max_us = 0U;
        bsp_hv_diag.raw_min = UINT16_MAX;
        bsp_hv_diag.raw_max = 0U;
        bsp_hv_diag.capture = 2U;
    }
    if (bsp_hv_diag.capture != 2U) return;
    ++bsp_hv_diag.samples;
    if (!valid) ++bsp_hv_diag.rejected;
    if (hv.ovp_suspended) ++bsp_hv_diag.ovp_holds;
    if ((flags & ADC_ISR_OVR) != 0U) ++bsp_hv_diag.overruns;
    if (raw < bsp_hv_diag.raw_min) bsp_hv_diag.raw_min = raw;
    if (raw > bsp_hv_diag.raw_max) bsp_hv_diag.raw_max = raw;
    bsp_hv_diag.interval_flags |= flags & 0xFFU;
    uint16_t irq_us = (uint16_t)((uint16_t)TIM1->CNT - started);
    if (irq_us > bsp_hv_diag.irq_max_us) bsp_hv_diag.irq_max_us = irq_us;
    if (++bsp_hv_diag.divider < bsp_hv_diag.decimation) return;
    bsp_hv_diag.divider = 0U;
    uint32_t i = bsp_hv_diag.count;
    bsp_hv_diag.records[i] = (bsp_hv_trace_t){
        .filtered_mv = bsp_hv_diag.filtered_mv,
        .raw_min = (uint16_t)bsp_hv_diag.raw_min,
        .raw_max = (uint16_t)bsp_hv_diag.raw_max,
        .compare = (uint16_t)TIM17->CCR1,
        .pwm_phase = phase,
        .flags = (uint16_t)(bsp_hv_diag.interval_flags | (valid ? 1U << 8U : 0U)
                           | (hv.ovp_suspended ? 1U << 9U : 0U)
                           | (((bsp_hv_diag.target_v - 100U) / 10U) << 11U)
                           | (hv.enabled ? 1U << 15U : 0U)
                           | ((BSP_SEPARATOR_ANODE_PORT->ODR & BSP_SEPARATOR_ANODE_PIN) != 0U ? 1U << 10U : 0U)),
        .irq_us = irq_us,
    };
    bsp_hv_diag.raw_min = UINT16_MAX;
    bsp_hv_diag.raw_max = 0U;
    bsp_hv_diag.interval_flags = 0U;
    bsp_hv_diag.count = i + 1U;
    if (i + 1U == BSP_HV_TRACE_COUNT) bsp_hv_diag.capture = 3U;
}
#endif

static uint32_t adc_to_voltage(uint16_t raw) {
    return ((uint32_t)raw * BSP_HV_VDDA_MV * BSP_HV_DIVIDER_RATIO + BSP_HV_ADC_MAX / 2U) / BSP_HV_ADC_MAX;
}

/* Both counters run from the same 48 MHz clock. An integer number of PWM
 * periods per sample removes the slow phase walk of an independent 2 kHz
 * trigger. At nominal duty the sample window lies inside MOSFET on-time,
 * away from switching edges. TIM17 has no internal trigger route to TIM3. */
static void synchronize_sampling(void) {
    uint32_t period = TIM17->ARR + 1U;
    uint32_t cycles = (PLATFORM_HCLK_HZ / BSP_HV_SAMPLE_RATE_HZ + period / 2U) / period;
    uint32_t sample_ticks = period * cycles;
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->ARR = sample_ticks - 1U;
    uint32_t phase = TIM17->CNT;
    uint32_t delay = BSP_HV_SAMPLE_OFFSET_TICKS >= phase ? BSP_HV_SAMPLE_OFFSET_TICKS - phase
                                                       : period - phase + BSP_HV_SAMPLE_OFFSET_TICKS;
    /* Next update in (cycles-1)*period + delay ticks. No UG: it would emit
     * an unwanted ADC trigger. PSC=0 needs no prescaler reload here. */
    TIM3->CNT = period - delay;
    TIM3->CR1 |= TIM_CR1_CEN;
    __set_PRIMASK(key);
}

static void pwm_disable(void) {
    /* Fault handlers may call this before TIM17 has been initialized. */
    if ((RCC->APB2ENR & RCC_APB2ENR_TIM17EN) != 0U) {
        /* Inhibit the output before resetting CCR1 and loading the preload. */
        TIM17->BDTR &= ~TIM_BDTR_MOE;
        TIM17->CCR1 = 0U;
        TIM17->EGR = TIM_EGR_UG;
        if (hv.initialized) {
            TIM17->SR = ~TIM_SR_UIF;
            TIM17->DIER |= TIM_DIER_UIE;
        }
        __DSB();
    }

    hv.duty_q16 = 0U;
    hv.enabled = false;
    hv.ovp_suspended = false;
    hv.ovp_samples = 0U;
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
        || SystemCoreClock != PLATFORM_HCLK_HZ || (RCC->CFGR & RCC_CFGR_PPRE) != 0U
        || TIM3->PSC != 0U || TIM3->ARR != BSP_TIM3_ARR
        || (TIM3->CR2 & TIM_CR2_MMS) != TIM_CR2_MMS_1 || (TIM3->CR1 & TIM_CR1_CEN) != 0U
        || TIM17->PSC != 0U || TIM17->ARR != BSP_TIM17_ARR
        || (ADC1->CFGR2 & ADC_CFGR2_CKMODE) != 0U || ADC1->CHSELR != ADC_CHSELR_CHSEL2 || ADC1->SMPR != BSP_HV_ADC_SAMPLE_TIME
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
    synchronize_sampling();

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
    bool ready = recoverable && hv.samples != 0U && hv.adc_raw < BSP_HV_OVP_RAW && (TIM17->CCER & TIM_CCER_CC1E) != 0U
                 && (TIM17->CR1 & TIM_CR1_CEN) != 0U && (ADC1->ISR & ADC_ISR_AWD) == 0U
                 && (uint32_t)(bsp_time_ms() - hv.last_sample_ms) <= BSP_HV_ADC_TIMEOUT_MS;

    if (ready) {
        if (!hv.enabled) {
            pwm_disable();
            hv.fault = BSP_HV_FAULT_NONE;
        }
        /* Only the ADC confirmation path may release a temporary OVP hold. */
        if (!hv.ovp_suspended) {
            TIM17->BDTR |= TIM_BDTR_MOE;
        }
        hv.enabled = true;
    }

    __set_PRIMASK(primask);
    return ready;
}

bool bsp_hv_set_duty_q16(uint32_t duty_q16) {
    if (!hv.initialized || duty_q16 > BSP_HV_DUTY_MAX_Q16) {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    bool allowed = hv.enabled && hv.fault == BSP_HV_FAULT_NONE;

    if (allowed) {
        uint32_t period = TIM17->ARR + 1U;

        uint32_t compare = (period * duty_q16 + BSP_HV_DUTY_SCALE / 2U) / BSP_HV_DUTY_SCALE;

        TIM17->CCR1 = compare;

        hv.duty_q16 = duty_q16;
    }

    __set_PRIMASK(primask);
    return allowed;
}

bool bsp_hv_set_frequency(uint32_t frequency_hz) {
    if (!hv.initialized || frequency_hz < BSP_HV_PWM_MIN_HZ || frequency_hz > BSP_HV_PWM_MAX_HZ) {
        return false;
    }

    uint32_t period = (BSP_HV_PWM_TICK_HZ + frequency_hz / 2U) / frequency_hz;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (TIM17->ARR != period - 1U) {
        /* Both registers have preload enabled. UDIS prevents a timer update
         * between the writes; masking IRQs excludes concurrent duty/fault writes.
         * The next overflow loads a matching pair without resetting CNT or MOE. */
        uint32_t cr1 = TIM17->CR1;
        TIM17->CR1 = cr1 | TIM_CR1_UDIS;
        TIM17->ARR = period - 1U;
        TIM17->CCR1 = (period * hv.duty_q16 + BSP_HV_DUTY_SCALE / 2U) / BSP_HV_DUTY_SCALE;
        TIM17->SR = ~TIM_SR_UIF;
        TIM17->DIER |= TIM_DIER_UIE;
        TIM17->CR1 = cr1;
    }

    __set_PRIMASK(primask);
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
    /* Snapshot registers under the lock, derive telemetry after releasing it.
     * ARR/CCR1 may be pending until the next PWM overflow. */
    uint32_t period = TIM17->ARR + 1U;
    uint32_t compare = hv.ovp_suspended ? 0U : TIM17->CCR1;
    status->adc_raw = hv.adc_raw;

    status->duty_limit_permille = BSP_HV_DUTY_MAX_PM;

    status->enabled = hv.enabled;
    status->fault = hv.fault;

    status->pwm_available = (TIM17->CCER & TIM_CCER_CC1E) != 0U && (TIM17->CR1 & TIM_CR1_CEN) != 0U;

    __set_PRIMASK(primask);
    status->pwm_frequency_hz = (BSP_HV_PWM_TICK_HZ + period / 2U) / period;
    status->duty_permille = (uint16_t)((compare * 1000U + period / 2U) / period);
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
#ifdef HV_DIAGNOSTICS
    uint16_t started = (uint16_t)TIM1->CNT;
    uint16_t phase = (uint16_t)TIM17->CNT;
#endif
    uint32_t flags = ADC1->ISR;
    uint32_t pending = flags & ADC1->IER;
    bool have_sample = (pending & ADC_ISR_EOC) != 0U;
    uint16_t raw = have_sample ? (uint16_t)ADC1->DR : hv.adc_raw;
    bool watchdog = (pending & ADC_ISR_AWD) != 0U;
    bool high = have_sample && raw >= BSP_HV_OVP_RAW;

    /* Stop energy input immediately, retaining CCR1 and the requested duty.
     * ADC freshness monitoring stays active while the output is held off. */
    if (watchdog || high) {
        TIM17->BDTR &= ~TIM_BDTR_MOE;
        __DSB();
        if (hv.enabled && hv.fault == BSP_HV_FAULT_NONE) {
            hv.ovp_suspended = true;
        }
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

    if (hv.fault == BSP_HV_FAULT_NONE) {
        if (high) {
            if (++hv.ovp_samples >= BSP_HV_OVP_CONFIRM_SAMPLES) {
                trip(BSP_HV_FAULT_OVERVOLTAGE);
            }
        } else {
            hv.ovp_samples = 0U;
            /* An AWD flag with a newer safe DR sample still needs a clean EOC.
             * Do not re-enable if another conversion has raised AWD meanwhile. */
            if (!watchdog && (ADC1->ISR & ADC_ISR_AWD) == 0U && hv.ovp_suspended && hv.enabled) {
                hv.ovp_suspended = false;
                TIM17->BDTR |= TIM_BDTR_MOE;
            }
        }
    }

    bool valid = !high && !watchdog && !hv.ovp_suspended;
    /* Suspect samples cannot change either the application's filter or PI. */
    if (hv.callback != NULL) {
        hv.callback(voltage, hv.fault, valid, hv.callback_arg);
    }
#ifdef HV_DIAGNOSTICS
    trace_sample(raw, flags, valid, phase, started);
#endif
}

static void adc_hw_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    (void)RCC->APB2ENR;
    /* Configuration only. Calibration and conversion start belong to BSP HV. */
    RCC->APB2RSTR |= RCC_APB2RSTR_ADC1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_ADC1RST;
    GPIOA->MODER |= GPIO_MODER_MODER2;
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR2;
    ADC1->IER = 0U;
    /* 12-bit, right aligned, one channel, rising TIM3 TRGO, overwrite on OVR. */
    ADC1->CFGR1 = ADC_CFGR1_EXTSEL_0 | ADC_CFGR1_EXTSEL_1 | ADC_CFGR1_EXTEN_0 | ADC_CFGR1_OVRMOD;
    ADC1->CFGR2 = 0U;          /* Asynchronous HSI14 / 1. */
    ADC1->SMPR = BSP_HV_ADC_SAMPLE_TIME;
    ADC1->CHSELR = ADC_CHSELR_CHSEL2;
    ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_EOSMP | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR | ADC_ISR_AWD;
    NVIC_ClearPendingIRQ(ADC1_COMP_IRQn);
    NVIC_SetPriority(ADC1_COMP_IRQn, BSP_IRQ_PRIORITY_HV);
    NVIC_EnableIRQ(ADC1_COMP_IRQn);
}

static void sample_timer_hw_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;
    TIM3->CR1 = 0U;
    TIM3->CR2 = 0U; /* Do not emit a trigger when loading PSC/ARR below. */
    TIM3->SMCR = 0U;
    TIM3->DIER = 0U;
    TIM3->PSC = 0U;
    TIM3->ARR = (uint32_t)BSP_TIM3_ARR;
    TIM3->CNT = 0U;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    TIM3->CR2 = TIM_CR2_MMS_1; /* Update event drives TRGO. */
}

static void pwm_hw_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
    (void)RCC->APB2ENR;
    TIM17->CR1 = TIM_CR1_ARPE;
    TIM17->CR2 = TIM_CR2_OIS1; /* HIGH idle keeps the MOSFET off. */
    TIM17->DIER = 0U;
    TIM17->PSC = 0U;
    TIM17->ARR = (uint32_t)BSP_TIM17_ARR;
    TIM17->RCR = 0U;
    TIM17->CNT = 0U;
    TIM17->CCR1 = 0U;
    TIM17->CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM17->BDTR = TIM_BDTR_OSSI | TIM_BDTR_BKP; /* MOE and AOE stay clear. */
    TIM17->CCER = TIM_CCER_CC1P | TIM_CCER_CC1E;
    TIM17->EGR = TIM_EGR_UG;
    TIM17->SR = 0U;
    NVIC_ClearPendingIRQ(TIM17_IRQn);
    NVIC_SetPriority(TIM17_IRQn, BSP_IRQ_PRIORITY_HV);
    NVIC_EnableIRQ(TIM17_IRQn);
    /* Connect AF only after the timer can hold the output in its HIGH idle state. */
    GPIOA->BSRR = GPIO_BSRR_BS_7;
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT_7;
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR7;
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR7;
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~GPIO_AFRL_AFSEL7) | (5U << GPIO_AFRL_AFSEL7_Pos);
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER7) | GPIO_MODER_MODER7_1;
}

void bsp_hv_hw_init(void) {
    adc_hw_init();
    sample_timer_hw_init();
    pwm_hw_init();
}

void bsp_hv_pwm_irq(void) {
    if ((TIM17->SR & TIM17->DIER & TIM_SR_UIF) == 0U) return;
    TIM17->DIER &= ~TIM_DIER_UIE;
    TIM17->SR = ~TIM_SR_UIF;
    if (hv.initialized) synchronize_sampling();
}
