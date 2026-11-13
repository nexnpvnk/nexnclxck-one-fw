#include "bsp_nixie.h"

#include "bsp_hw.h"
#include "bsp_pins.h"

#include <stddef.h>

typedef struct {
    GPIO_TypeDef* port;
    uint32_t pin;
} pin_t;

static const pin_t anodes[] = {BSP_NIXIE_ANODE_BINDINGS};
static const pin_t cathodes[] = {BSP_NIXIE_CATHODE_BINDINGS};

enum {
    ANODE_COUNT = sizeof(anodes) / sizeof(anodes[0]),
    CATHODE_COUNT = sizeof(cathodes) / sizeof(cathodes[0]),
};

static const nixie_caps_t caps = {
    .anode_count = ANODE_COUNT, .cathode_count = CATHODE_COUNT, .max_slot_us = UINT16_MAX};

_Static_assert(ANODE_COUNT == 8U, "Carrier has eight anodes");
_Static_assert(CATHODE_COUNT == 16U, "Carrier has sixteen cathodes");

typedef struct {
    uint32_t cathode_b;
    uint32_t cathode_c;
    GPIO_TypeDef* anode_port;
    uint16_t anode_pin;
    uint16_t on_us;
} hw_slot_t;

typedef struct {
    nixie_timing_t timing;
    uint16_t slot_us;
    uint8_t slot_count;
    uint8_t rtc_slots; /* Lit slots gated by PC13; dark slots keep their time. */
    hw_slot_t slots[ANODE_COUNT];
} hw_plan_t;

typedef enum { SLOT_BLANKING, SLOT_ON, SLOT_DARK } slot_state_t;

/* Only IRQ reads the active plan. Foreground writes the other one exclusively,
 * then publishes its index in a short interrupt-masked section. */
static hw_plan_t plans[2];
static volatile uint8_t active;
static volatile bool pending;
static volatile bool have_plan;
static volatile bool prepared;
static volatile bool initialized;
static volatile bool running;
static volatile bool rtc_phase_on;
static volatile bool shutdown_latched;
static volatile uint32_t frame_count;
static volatile uint32_t missed_slots;
static volatile uint16_t max_irq_delay_us;
static uint8_t slot_index;
static slot_state_t slot_state;
static uint16_t phase_deadline;

static void scan_anodes_off(void) {
    GPIOA->BSRR = BSP_NIXIE_ANODE_A_MASK << 16U;
    GPIOB->BSRR = BSP_NIXIE_ANODE_B_MASK << 16U;
    GPIOC->BSRR = BSP_NIXIE_ANODE_C_MASK << 16U;
    __DSB();
}

static void scan_cathodes_off(void) {
    GPIOB->BSRR = BSP_NIXIE_CATHODE_B_MASK << 16U;
    GPIOC->BSRR = BSP_NIXIE_CATHODE_C_MASK << 16U;
    __DSB();
}

/* Called with IRQs masked: common HV disable and latched fault stop. */
static void all_outputs_off(void) {
    GPIOA->BSRR = BSP_NIXIE_ANODE_A_MASK << 16U;
    GPIOB->BSRR = BSP_NIXIE_ANODE_B_MASK << 16U;
    GPIOC->BSRR = BSP_NIXIE_ANODE_C_MASK << 16U;
    __DSB();
    GPIOB->BSRR = BSP_NIXIE_CATHODE_B_MASK << 16U;
    GPIOC->BSRR = BSP_NIXIE_CATHODE_C_MASK << 16U;
    __DSB();
}

/* One RTC phase per complete frame; no foreground blink timer or EXTI needed. */
static void sample_rtc_phase(void) { rtc_phase_on = (BSP_RTC_LED_PORT->IDR & BSP_RTC_LED_PIN) == 0U; }

static void timer_stop(void) {
    TIM1->DIER = 0U;
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->SR = 0U;
    NVIC_ClearPendingIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
}

/* IRQs are masked by the caller. Free-running TIM1, modulo 16 bits; every
 * nonzero phase fits 65535 us. Deadlines follow the actual GPIO transition,
 * never an old slot boundary. A late start cannot truncate the on pulse. */
static void arm_phase(uint16_t duration_us) {
    uint16_t start = (uint16_t)TIM1->CNT;
    phase_deadline = (uint16_t)(start + duration_us);
    TIM1->CCR1 = phase_deadline;
    TIM1->SR = ~TIM_SR_CC1IF;
    TIM1->DIER = TIM_DIER_CC1IE;
    /* Preserve arbitrarily short nonzero pulses: if programming consumed the
     * interval, request the transition now rather than wait a whole wrap. */
    if ((uint16_t)((uint16_t)TIM1->CNT - start) >= duration_us) {
        TIM1->EGR = TIM_EGR_CC1G;
    }
}

static void illuminate_slot(void) {
    const hw_slot_t* slot = &plans[active].slots[slot_index];
    GPIOB->BSRR = slot->cathode_b;
    GPIOC->BSRR = slot->cathode_c;
    __DSB();
    slot->anode_port->BSRR = slot->anode_pin;
    __DSB();
    slot_state = SLOT_ON;
    arm_phase(slot->on_us);
}

/* All unused PWM time precedes the pulse, so off_us is at least the configured
 * TLP127 blanking time. Dark slots still reserve their complete duration. */
static void begin_slot(void) {
    const hw_plan_t* plan = &plans[active];
    const hw_slot_t* slot = &plan->slots[slot_index];
    if (slot->on_us == 0U || ((plan->rtc_slots & (1U << slot_index)) != 0U && !rtc_phase_on)) {
        slot_state = SLOT_DARK;
        arm_phase(plan->slot_us);
    } else {
        uint16_t off_us = plan->slot_us - slot->on_us;
        if (off_us == 0U) {
            illuminate_slot();
        } else {
            slot_state = SLOT_BLANKING;
            arm_phase(off_us);
        }
    }
}

static void timer_start(void) {
    timer_stop();
    scan_anodes_off();
    scan_cathodes_off();
    slot_index = 0U;
    sample_rtc_phase();
    running = true;
    TIM1->CNT = 0U;
    TIM1->SR = 0U;
    TIM1->CR1 |= TIM_CR1_CEN;
    begin_slot();
}

void bsp_nixie_hw_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;
    RCC->APB2RSTR |= RCC_APB2RSTR_TIM1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM1RST;
    TIM1->CR1 = 0U;
    TIM1->CR2 = 0U;
    TIM1->SMCR = 0U;
    TIM1->DIER = 0U;
    TIM1->PSC = (uint32_t)BSP_TIM_US_PSC;
    TIM1->ARR = UINT16_MAX;
    TIM1->RCR = 0U;
    TIM1->CCMR1 = 0U;
    TIM1->CCMR2 = 0U;
    TIM1->CCER = 0U;
    TIM1->BDTR = 0U;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0U;
    NVIC_ClearPendingIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
    NVIC_SetPriority(TIM1_CC_IRQn, BSP_IRQ_PRIORITY_REALTIME);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
    prepared = true;
}

bool bsp_nixie_init(void) {
    if (!prepared || shutdown_latched || (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) == 0U || TIM1->PSC != BSP_TIM_US_PSC
        || TIM1->ARR != UINT16_MAX) {
        return false;
    }
    initialized = true;
    return true;
}

const nixie_caps_t* bsp_nixie_caps(void) { return &caps; }

nixie_status_t bsp_nixie_present(const nixie_plan_t* plan, uint16_t rtc_anodes) {
    if (__get_IPSR() != 0U || __get_PRIMASK() != 0U || !initialized || shutdown_latched) {
        return NIXIE_NOT_READY;
    }
    nixie_status_t result = nixie_validate_plan(plan, &caps);
    if (result != NIXIE_OK) {
        return result;
    }
    uint16_t anode_mask = 0U;
    for (uint8_t s = 0U; s < plan->slot_count; ++s) {
        anode_mask |= (uint16_t)(1U << plan->slots[s].anode);
    }
    if ((rtc_anodes & ~anode_mask) != 0U) {
        return NIXIE_INVALID_WIRING;
    }
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    bool busy = pending;
    uint8_t next = active ^ 1U;
    __set_PRIMASK(key);
    if (busy) {
        return NIXIE_BUSY;
    }
    hw_plan_t* target = &plans[next];
    target->timing = plan->timing;
    target->slot_us = plan->slot_us;
    target->slot_count = plan->slot_count;
    target->rtc_slots = 0U;
    for (uint8_t s = 0U; s < plan->slot_count; ++s) {
        const nixie_slot_t* source = &plan->slots[s];
        if (source->on_us != 0U && (rtc_anodes & (1U << source->anode)) != 0U) {
            target->rtc_slots |= (uint8_t)(1U << s);
        }
        target->slots[s] = (hw_slot_t){
            .anode_port = anodes[source->anode].port,
            .anode_pin = (uint16_t)anodes[source->anode].pin,
            .on_us = source->on_us,
        };
        uint32_t b = 0U;
        uint32_t c = 0U;
        for (uint8_t channel = 0U; channel < caps.cathode_count; ++channel) {
            if ((source->cathodes & (1U << channel)) != 0U) {
                if (cathodes[channel].port == GPIOB) {
                    b |= cathodes[channel].pin;
                } else {
                    c |= cathodes[channel].pin;
                }
            }
        }
        target->slots[s].cathode_b = b | ((BSP_NIXIE_CATHODE_B_MASK & ~b) << 16U);
        target->slots[s].cathode_c = c | ((BSP_NIXIE_CATHODE_C_MASK & ~c) << 16U);
    }
    key = __get_PRIMASK();
    __disable_irq();
    if (shutdown_latched) {
        __set_PRIMASK(key);
        return NIXIE_NOT_READY;
    }
    __DMB();
    if (running) {
        pending = true;
    } else {
        active = next;
        have_plan = true;
    }
    __set_PRIMASK(key);
    return NIXIE_OK;
}

bool bsp_nixie_set_enabled(bool enabled) {
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    if (!enabled) {
        if (prepared) {
            timer_stop();
            all_outputs_off();
        }
        running = false;
        if (pending) {
            active ^= 1U;
            pending = false;
            have_plan = true;
        }
    } else if (!initialized || !have_plan || shutdown_latched) {
        __set_PRIMASK(key);
        return false;
    } else {
        if (!running) {
            timer_start();
        }
    }
    __set_PRIMASK(key);
    return true;
}

void bsp_nixie_shutdown(void) {
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    shutdown_latched = true;
    running = false;
    pending = false;
    if ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) != 0U) {
        timer_stop();
    }
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN;
    (void)RCC->AHBENR;
    all_outputs_off();
    __set_PRIMASK(key);
}

bool bsp_nixie_get_status(bsp_nixie_status_t* status) {
    if (status == NULL) {
        return false;
    }
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    const hw_plan_t* plan = &plans[active];
    uint32_t period = have_plan ? (uint32_t)plan->slot_us * plan->slot_count : 0U;
    *status = (bsp_nixie_status_t){
        .frames = frame_count,
        .missed_slots = missed_slots,
        .max_irq_delay_us = max_irq_delay_us,
        .running = running,
        .separators_on = running && rtc_phase_on && plan->rtc_slots != 0U,
        .shutdown = shutdown_latched,
    };
    __set_PRIMASK(key);
    status->refresh_rate_millihz = period != 0U ? 1000000000UL / period : 0U;
    return initialized;
}

void bsp_nixie_timer_irq(void) {
    if ((TIM1->SR & TIM1->DIER & TIM_SR_CC1IF) == 0U) {
        return;
    }
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    TIM1->DIER = 0U;
    TIM1->SR = ~TIM_SR_CC1IF;
    if (!running || shutdown_latched) {
        scan_anodes_off();
        __set_PRIMASK(key);
        return;
    }
    /* End the pulse before statistics and frame bookkeeping. */
    if (slot_state != SLOT_BLANKING) {
        scan_anodes_off();
    }
    uint16_t delay_us = (uint16_t)((uint16_t)TIM1->CNT - phase_deadline);
    if (delay_us > max_irq_delay_us) {
        max_irq_delay_us = delay_us;
    }
    if (delay_us >= plans[active].slot_us) {
        ++missed_slots;
    }
    if (slot_state == SLOT_BLANKING) {
        illuminate_slot();
    } else {
        ++slot_index;
        if (slot_index == plans[active].slot_count) {
            slot_index = 0U;
            ++frame_count;
            if (pending) {
                active ^= 1U;
                __DMB();
                pending = false;
            }
            sample_rtc_phase();
        }
        begin_slot();
    }
    __set_PRIMASK(key);
}
