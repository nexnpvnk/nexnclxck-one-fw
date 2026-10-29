#include "bsp_buttons.h"

#include "stm32f0xx.h"

#define BUTTON_PINS_MASK 0x0FU

static volatile uint8_t pressed_mask;

static void buttons_capture(void) { pressed_mask = (uint8_t)(~GPIOC->IDR & BUTTON_PINS_MASK); }

void bsp_buttons_init(void) { buttons_capture(); }

uint8_t bsp_buttons_get_pressed_mask(void) { return pressed_mask; }

void bsp_buttons_irq(uint32_t lines) {
    uint32_t pending = EXTI->PR & lines & BUTTON_PINS_MASK;
    if (pending != 0U) {
        EXTI->PR = pending;
        buttons_capture();
    }
}
