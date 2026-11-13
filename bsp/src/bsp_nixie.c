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

#define TIMER_FLAGS (TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC2IF)

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
    hw_slot_t slots[ANODE_COUNT];
} hw_plan_t;

/* WAIT_UPDATE covers both completed PWM and full brightness held until update. */
typedef enum { SLOT_SKIPPED, SLOT_BLANKING, SLOT_ON, SLOT_WAIT_UPDATE } slot_state_t;

/* Only IRQ reads the active plan. Foreground writes the other one exclusively,
 * then publishes its index in a short interrupt-masked section. */
static hw_plan_t plans[2];
static volatile uint8_t active;
static volatile bool pending;
static volatile bool have_plan;
static volatile bool prepared;
static volatile bool initialized;
static volatile bool running;
static volatile bool outputs_enabled;
static volatile bool separators_enabled;
static volatile bool separators_on;
static volatile bool shutdown_latched;
static volatile uint32_t frame_count;
static volatile uint32_t missed_slots;
static uint8_t slot_index;
static slot_state_t slot_state;

/* Scan operations must never touch the static separator outputs. */
static void scan_anodes_off(void) {
    GPIOA->BSRR = BSP_NIXIE_ANODE_A_MASK << 16U;
    GPIOB->BSRR = BSP_NIXIE_SCAN_ANODE_B_MASK << 16U;
    GPIOC->BSRR = BSP_NIXIE_ANODE_C_MASK << 16U;
    __DSB();
}

static void scan_cathodes_off(void) {
    GPIOB->BSRR = BSP_NIXIE_SCAN_CATHODE_B_MASK << 16U;
    GPIOC->BSRR = BSP_NIXIE_SCAN_CATHODE_C_MASK << 16U;
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
    separators_on = false;
}

/* Called with IRQs masked. RTC owns the phase; no software blink timer or PWM.
 * Only reserved GPIO bits are written, preserving the current digit slot. */
static void apply_separators(void) {
    bool on =
        outputs_enabled && separators_enabled && !shutdown_latched && (BSP_RTC_LED_PORT->IDR & BSP_RTC_LED_PIN) == 0U;
    if (on == separators_on) {
        return;
    }
    if (on) {
        GPIOB->BSRR = BSP_SEPARATOR_CATHODE_B_MASK;
        GPIOC->BSRR = BSP_SEPARATOR_CATHODE_C_MASK;
        __DSB();
        BSP_SEPARATOR_ANODE_PORT->BSRR = BSP_SEPARATOR_ANODE_PIN;
    } else {
        BSP_SEPARATOR_ANODE_PORT->BSRR = BSP_SEPARATOR_ANODE_PIN << 16U;
        __DSB();
        GPIOB->BSRR = BSP_SEPARATOR_CATHODE_B_MASK << 16U;
        GPIOC->BSRR = BSP_SEPARATOR_CATHODE_C_MASK << 16U;
    }
    __DSB();
    separators_on = on;
}

static void timer_stop(void) {
    TIM1->DIER = 0U;
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->SR = 0U;
    NVIC_ClearPendingIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
}

static void timer_restart(uint16_t slot_us) {
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->ARR = (uint32_t)slot_us - 1U;
    TIM1->CNT = 0U;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0U;
    TIM1->CR1 |= TIM_CR1_CEN;
}

/* Compare IRQs are already disabled by arm_slot/illuminate_slot. */
static void skip_slot(void) {
    scan_anodes_off();
    if (slot_state != SLOT_SKIPPED) {
        ++missed_slots;
    }
    slot_state = SLOT_SKIPPED;
    /* Preserve an update which may have arrived during this handler. */
    TIM1->SR = ~(TIM_SR_CC1IF | TIM_SR_CC2IF);
}

/* Blanking time has elapsed. Change cathodes only now, after the TLP127 turn-off
 * interval. Arm PWM from the actual anode-on write without fixed IRQ margins. */
static void illuminate_slot(void) {
    const hw_plan_t* plan = &plans[active];
    const hw_slot_t* slot = &plan->slots[slot_index];
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    TIM1->DIER = TIM_DIER_UIE;
    TIM1->SR = ~(TIM_SR_CC1IF | TIM_SR_CC2IF);
    if ((TIM1->SR & TIM_SR_UIF) != 0U) {
        skip_slot();
        __set_PRIMASK(key);
        return;
    }
    GPIOB->BSRR = slot->cathode_b;
    GPIOC->BSRR = slot->cathode_c;
    __DSB();
    slot->anode_port->BSRR = slot->anode_pin;
    __DSB();
    uint32_t end = TIM1->CNT + slot->on_us;
    if ((TIM1->SR & TIM_SR_UIF) != 0U) {
        skip_slot();
    } else if (end >= plan->slot_us) {
        /* Full brightness (or a delayed start): the next update ends the pulse. */
        slot_state = SLOT_WAIT_UPDATE;
    } else {
        TIM1->CCR2 = end;
        TIM1->SR = ~TIM_SR_CC2IF;
        slot_state = SLOT_ON;
        TIM1->DIER = TIM_DIER_UIE | TIM_DIER_CC2IE;
        /* A short pulse may already have ended while compare was programmed.
         * Finish it immediately instead of imposing a minimum PWM width. */
        if (TIM1->CNT >= end || (TIM1->SR & TIM_SR_UIF) != 0U) {
            scan_anodes_off();
            TIM1->DIER = TIM_DIER_UIE;
            TIM1->SR = ~TIM_SR_CC2IF;
            slot_state = SLOT_WAIT_UPDATE;
        }
    }
    __set_PRIMASK(key);
}

/* Only update IRQ is enabled on entry. off_time follows the GPIO-off writes.
 * After a counter restart, pass 0: the anodes were already off before restart. */
static void arm_slot(uint32_t off_time) {
    const hw_plan_t* plan = &plans[active];
    const hw_slot_t* slot = &plan->slots[slot_index];
    if (slot->on_us == 0U) {
        slot_state = SLOT_WAIT_UPDATE;
        return;
    }
    /* Slot/frame bookkeeping consumes part of the blanking interval instead of
     * postponing its start and taking more time from the illumination window. */
    uint32_t start = off_time + plan->timing.blanking_time_us;
    slot_state = SLOT_BLANKING;
    if (start >= plan->slot_us || (TIM1->SR & TIM_SR_UIF) != 0U) {
        skip_slot();
        return;
    }
    if (plan->timing.blanking_time_us == 0U) {
        illuminate_slot();
        return;
    }
    TIM1->CCR1 = start;
    TIM1->SR = ~TIM_SR_CC1IF;
    TIM1->DIER = TIM_DIER_UIE | TIM_DIER_CC1IE;
    /* Handle a compare already reached during register programming, without
     * waiting or dropping a slot just because blanking time is short. */
    if (TIM1->CNT >= start || (TIM1->SR & TIM_SR_UIF) != 0U) {
        illuminate_slot();
    }
}

static void timer_start(void) {
    const hw_plan_t* plan = &plans[active];
    timer_stop();
    scan_anodes_off();
    scan_cathodes_off();
    slot_index = 0U;
    running = true;
    timer_restart(plan->slot_us);
    TIM1->DIER = TIM_DIER_UIE;
    arm_slot(0U);
}

void bsp_nixie_hw_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGCOMPEN;
    (void)RCC->APB2ENR;
    RCC->APB2RSTR |= RCC_APB2RSTR_TIM1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM1RST;
    TIM1->CR1 = TIM_CR1_ARPE;
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
    NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 2U);
    NVIC_SetPriority(TIM1_CC_IRQn, 2U);
    NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
    /* Observe the existing RTC_CALIB output on PC13, on both phase edges.
     * Only EXTI13 is owned here; buttons keep EXTI0..3. */
    EXTI->IMR &= ~EXTI_IMR_MR13;
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~SYSCFG_EXTICR4_EXTI13) | SYSCFG_EXTICR4_EXTI13_PC;
    EXTI->RTSR |= EXTI_RTSR_TR13;
    EXTI->FTSR |= EXTI_FTSR_TR13;
    EXTI->PR = EXTI_PR_PR13;
    NVIC_ClearPendingIRQ(EXTI4_15_IRQn);
    NVIC_SetPriority(EXTI4_15_IRQn, 3U);
    EXTI->IMR |= EXTI_IMR_MR13;
    NVIC_EnableIRQ(EXTI4_15_IRQn);
    prepared = true;
}

bool bsp_nixie_init(void) {
    if (!prepared || shutdown_latched || (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) == 0U || TIM1->PSC != BSP_TIM_US_PSC) {
        return false;
    }
    initialized = true;
    return true;
}

const nixie_caps_t* bsp_nixie_caps(void) { return &caps; }

nixie_status_t bsp_nixie_validate_board(const nixie_board_t* board, nixie_issue_t* issue) {
    nixie_status_t result = nixie_validate_board(board, &caps, issue);
    if (result != NIXIE_OK) {
        return result;
    }
    for (uint8_t e = 0U; e < board->element_count; ++e) {
        const nixie_element_t* element = &board->elements[e];
        bool reserved = element->anode_channel == BSP_SEPARATOR_ANODE_CHANNEL;
        for (uint8_t c = 0U; c < element->tube->cathode_count; ++c) {
            uint8_t channel = element->cathode_channels[c];
            if (channel != NIXIE_CHANNEL_FIXED && (BSP_SEPARATOR_CATHODE_CHANNELS & (1U << channel)) != 0U) {
                reserved = true;
            }
        }
        if (reserved) {
            if (issue != NULL) {
                *issue = (nixie_issue_t){NIXIE_INVALID_WIRING, e};
            }
            return NIXIE_INVALID_WIRING;
        }
    }
    return NIXIE_OK;
}

nixie_status_t bsp_nixie_present(const nixie_plan_t* plan) {
    if (__get_IPSR() != 0U || __get_PRIMASK() != 0U || !initialized || shutdown_latched) {
        return NIXIE_NOT_READY;
    }
    nixie_status_t result = nixie_validate_plan(plan, &caps);
    if (result != NIXIE_OK) {
        return result;
    }
    for (uint8_t s = 0U; s < plan->slot_count; ++s) {
        if (plan->slots[s].anode == BSP_SEPARATOR_ANODE_CHANNEL
            || (plan->slots[s].cathodes & BSP_SEPARATOR_CATHODE_CHANNELS) != 0U) {
            return NIXIE_INVALID_WIRING;
        }
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
    for (uint8_t s = 0U; s < plan->slot_count; ++s) {
        const nixie_slot_t* source = &plan->slots[s];
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
        target->slots[s].cathode_b = b | ((BSP_NIXIE_SCAN_CATHODE_B_MASK & ~b) << 16U);
        target->slots[s].cathode_c = c | ((BSP_NIXIE_SCAN_CATHODE_C_MASK & ~c) << 16U);
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
        outputs_enabled = false;
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
        outputs_enabled = true;
        if (!running) {
            timer_start();
        }
        apply_separators();
    }
    __set_PRIMASK(key);
    return true;
}

bool bsp_nixie_set_separators(bool enabled) {
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    if (!initialized || shutdown_latched) {
        __set_PRIMASK(key);
        return false;
    }
    separators_enabled = enabled;
    apply_separators();
    __set_PRIMASK(key);
    return true;
}

void bsp_nixie_shutdown(void) {
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    shutdown_latched = true;
    outputs_enabled = false;
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
        .running = running,
        .separators_on = separators_on,
        .shutdown = shutdown_latched,
    };
    __set_PRIMASK(key);
    status->refresh_rate_millihz = period != 0U ? 1000000000UL / period : 0U;
    return initialized;
}

void bsp_nixie_timer_irq(void) {
    uint32_t flags = TIM1->SR & TIM1->DIER & TIMER_FLAGS;
    if (flags == 0U) {
        return;
    }
    if (!running || shutdown_latched) {
        TIM1->SR = ~flags;
        scan_anodes_off();
        return;
    }
    if ((flags & TIM_SR_UIF) != 0U) {
        uint32_t key = __get_PRIMASK();
        __disable_irq();
        scan_anodes_off();
        uint32_t off_time = TIM1->CNT;
        TIM1->DIER = TIM_DIER_UIE;
        /* Acknowledge this update before allowing preemption. A later update
         * must stay pending throughout slot/frame bookkeeping. */
        TIM1->SR = ~TIMER_FLAGS;
        __set_PRIMASK(key);
        if (slot_state == SLOT_BLANKING || slot_state == SLOT_ON) {
            ++missed_slots;
        }
        ++slot_index;
        const hw_plan_t* previous = &plans[active];
        if (slot_index == previous->slot_count) {
            slot_index = 0U;
            ++frame_count;
            if (pending) {
                uint16_t old_period = previous->slot_us;
                active ^= 1U;
                __DMB();
                pending = false;
                if (plans[active].slot_us != old_period) {
                    /* Change ARR only at a blank complete-frame boundary. */
                    timer_restart(plans[active].slot_us);
                    arm_slot(0U);
                    return;
                }
            }
        }
        arm_slot(off_time);
        return;
    }

    if ((flags & TIM_SR_CC2IF) != 0U) {
        scan_anodes_off();
        TIM1->DIER = TIM_DIER_UIE;
        TIM1->SR = ~TIM_SR_CC2IF;
        slot_state = SLOT_WAIT_UPDATE;
    } else if ((flags & TIM_SR_CC1IF) != 0U) {
        if (slot_state == SLOT_BLANKING) {
            illuminate_slot();
        } else {
            TIM1->DIER = TIM_DIER_UIE;
            TIM1->SR = ~TIM_SR_CC1IF;
        }
    }
}

void bsp_nixie_separator_irq(void) {
    if ((EXTI->PR & EXTI_PR_PR13) == 0U) {
        return;
    }
    EXTI->PR = EXTI_PR_PR13;
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    apply_separators();
    __set_PRIMASK(key);
}
