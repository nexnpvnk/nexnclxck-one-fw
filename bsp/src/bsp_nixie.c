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
static const nixie_caps_t caps = {8U, 16U, UINT16_MAX, 20U};

_Static_assert(sizeof(anodes) / sizeof(anodes[0]) == 8U, "Carrier has eight anodes");
_Static_assert(sizeof(cathodes) / sizeof(cathodes[0]) == 16U, "Carrier has sixteen cathodes");

#define TIMER_FLAGS (TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC2IF | TIM_SR_CC3IF)
#define TIMER_IRQS  (TIM_DIER_UIE | TIM_DIER_CC1IE | TIM_DIER_CC2IE | TIM_DIER_CC3IE)

typedef struct {
    uint32_t cathode_b;
    uint32_t cathode_c;
    uint16_t on_us;
    uint8_t anode;
} hw_slot_t;

typedef struct {
    nixie_timing_t timing;
    uint16_t slot_us;
    uint8_t slot_count;
    hw_slot_t slots[NIXIE_MAX_SLOTS];
} hw_plan_t;

typedef enum { SLOT_SKIPPED, SLOT_CATHODES, SLOT_ANODE, SLOT_OFF, SLOT_DONE } slot_state_t;

/* Only IRQ reads the active plan. Foreground writes the other one exclusively,
 * then publishes its index in a short interrupt-masked section. */
static hw_plan_t plans[2];
static volatile uint8_t active;
static volatile bool pending;
static volatile bool have_plan;
static volatile bool prepared;
static volatile bool initialized;
static volatile bool running;
static volatile bool shutdown_latched;
static volatile uint32_t frame_count;
static volatile uint32_t missed_slots;
static uint8_t slot_index;
static slot_state_t slot_state;
static uint16_t anodes_off_at;
static uint16_t cathodes_set_at;

static void anodes_off(void) {
    GPIOA->BSRR = BSP_NIXIE_ANODE_A_MASK << 16U;
    GPIOB->BSRR = BSP_NIXIE_ANODE_B_MASK << 16U;
    GPIOC->BSRR = BSP_NIXIE_ANODE_C_MASK << 16U;
    __DSB();
}

static void cathodes_off(void) {
    GPIOB->BSRR = BSP_NIXIE_CATHODE_B_MASK << 16U;
    GPIOC->BSRR = BSP_NIXIE_CATHODE_C_MASK << 16U;
    __DSB();
}

static void timer_stop(void) {
    TIM1->DIER = 0U;
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->SR = 0U;
    NVIC_ClearPendingIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
}

static void skip_slot(void) {
    anodes_off();
    if (slot_state != SLOT_SKIPPED) {
        ++missed_slots;
    }
    slot_state = SLOT_SKIPPED;
    TIM1->DIER = TIM_DIER_UIE;
    /* Preserve an update which may have arrived during this handler. */
    TIM1->SR = ~(TIM_SR_CC1IF | TIM_SR_CC2IF | TIM_SR_CC3IF);
}

static bool event_on_time(uint16_t now, uint16_t target, const hw_plan_t* plan) {
    return now >= target && (uint32_t)now - target <= plan->timing.max_lateness_us && (TIM1->SR & TIM_SR_UIF) == 0U;
}

/* Called after all anodes are off, with any old compare flags cleared. */
static void arm_slot(void) {
    const hw_plan_t* plan = &plans[active];
    const hw_slot_t* slot = &plan->slots[slot_index];
    TIM1->DIER = TIM_DIER_UIE;
    if (slot->on_us == 0U) {
        slot_state = SLOT_DONE;
        return;
    }
    TIM1->CCR1 = (uint32_t)plan->timing.turn_off_us + plan->timing.max_lateness_us;
    /* Reserve update-IRQ latency so dead_time is a minimum measured from the
     * actual anode-off write, rather than merely from the timer boundary. */
    TIM1->CCR2 = (uint32_t)plan->timing.dead_time_us + plan->timing.max_lateness_us;
    TIM1->CCR3 = TIM1->CCR2 + slot->on_us;
    slot_state = SLOT_CATHODES;
    if (TIM1->CNT > plan->timing.max_lateness_us || (TIM1->SR & TIM_SR_UIF) != 0U) {
        skip_slot();
        return;
    }
    TIM1->DIER = TIMER_IRQS;
}

static void timer_start(void) {
    const hw_plan_t* plan = &plans[active];
    timer_stop();
    anodes_off();
    cathodes_off();
    TIM1->ARR = (uint32_t)plan->slot_us - 1U;
    TIM1->CNT = 0U;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0U;
    slot_index = 0U;
    anodes_off_at = 0U;
    arm_slot();
    running = true;
    TIM1->CR1 |= TIM_CR1_CEN;
}

void bsp_nixie_hw_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
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

nixie_status_t bsp_nixie_present(const nixie_plan_t* plan) {
    if (__get_IPSR() != 0U || __get_PRIMASK() != 0U || !initialized || shutdown_latched) {
        return NIXIE_NOT_READY;
    }
    nixie_status_t result = nixie_validate_plan(plan, &caps);
    if (result != NIXIE_OK) {
        return result;
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
        target->slots[s] = (hw_slot_t){
            .cathode_b = b | ((BSP_NIXIE_CATHODE_B_MASK & ~b) << 16U),
            .cathode_c = c | ((BSP_NIXIE_CATHODE_C_MASK & ~c) << 16U),
            .on_us = source->on_us,
            .anode = source->anode,
        };
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
            anodes_off();
            cathodes_off();
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
    } else if (!running) {
        timer_start();
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
    anodes_off();
    cathodes_off();
    __set_PRIMASK(key);
}

bool bsp_nixie_get_status(bsp_nixie_status_t* status) {
    if (status == NULL) {
        return false;
    }
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    uint32_t period = have_plan ? (uint32_t)plans[active].slot_us * plans[active].slot_count : 0U;
    *status = (bsp_nixie_status_t){
        .frames = frame_count, .missed_slots = missed_slots, .running = running, .shutdown = shutdown_latched};
    __set_PRIMASK(key);
    status->refresh_millihz = period != 0U ? 1000000000UL / period : 0U;
    return initialized;
}

void bsp_nixie_timer_irq(void) {
    uint32_t flags = TIM1->SR & TIM1->DIER & TIMER_FLAGS;
    if (flags == 0U) {
        return;
    }
    if (!running || shutdown_latched) {
        TIM1->SR = ~flags;
        anodes_off();
        return;
    }
    if ((flags & TIM_SR_UIF) != 0U) {
        anodes_off();
        anodes_off_at = (uint16_t)TIM1->CNT;
        if (slot_state != SLOT_DONE && slot_state != SLOT_SKIPPED) {
            ++missed_slots;
        }
        TIM1->SR = ~TIMER_FLAGS;
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
                    TIM1->CR1 &= ~TIM_CR1_CEN;
                    TIM1->ARR = (uint32_t)plans[active].slot_us - 1U;
                    TIM1->CNT = 0U;
                    TIM1->EGR = TIM_EGR_UG;
                    TIM1->SR = 0U;
                    anodes_off_at = 0U;
                    arm_slot();
                    TIM1->CR1 |= TIM_CR1_CEN;
                    return;
                }
            }
        }
        arm_slot();
        return;
    }

    const hw_plan_t* plan = &plans[active];
    const hw_slot_t* slot = &plan->slots[slot_index];
    uint16_t now = (uint16_t)TIM1->CNT;
    /* Do not replay accumulated phases after a delayed IRQ. */
    if ((flags & (flags - 1U)) != 0U) {
        skip_slot();
        return;
    }
    TIM1->SR = ~flags;
    if (flags == TIM_SR_CC3IF) {
        anodes_off();
        now = (uint16_t)TIM1->CNT;
        if (slot_state != SLOT_OFF || !event_on_time(now, (uint16_t)TIM1->CCR3, plan)) {
            skip_slot();
        } else {
            slot_state = SLOT_DONE;
        }
    } else if (flags == TIM_SR_CC1IF && slot_state == SLOT_CATHODES && event_on_time(now, (uint16_t)TIM1->CCR1, plan)
               && (uint32_t)now >= (uint32_t)anodes_off_at + plan->timing.turn_off_us) {
        GPIOB->BSRR = slot->cathode_b;
        GPIOC->BSRR = slot->cathode_c;
        __DSB();
        cathodes_set_at = (uint16_t)TIM1->CNT;
        slot_state = SLOT_ANODE;
    } else if (flags == TIM_SR_CC2IF && slot_state == SLOT_ANODE) {
        /* A higher-priority IRQ must not intervene between checking the deadline
         * and asserting an anode. This section contains no loops or callbacks. */
        uint32_t key = __get_PRIMASK();
        __disable_irq();
        now = (uint16_t)TIM1->CNT;
        if (event_on_time(now, (uint16_t)TIM1->CCR2, plan)
            && (uint32_t)now >= (uint32_t)anodes_off_at + plan->timing.dead_time_us
            && (uint32_t)now >= (uint32_t)cathodes_set_at + plan->timing.cathode_setup_us
            && (TIM1->SR & TIM_SR_CC3IF) == 0U) {
            anodes[slot->anode].port->BSRR = anodes[slot->anode].pin;
            __DSB();
            slot_state = SLOT_OFF;
        } else {
            skip_slot();
        }
        __set_PRIMASK(key);
    } else {
        skip_slot();
    }
}
