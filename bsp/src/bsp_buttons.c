#include "bsp_buttons.h"

#include "stm32f0xx_hal.h"

#define BUTTON_PINS_MASK 0x0FU

static volatile uint8_t pressed_mask;

static void buttons_capture(void) { pressed_mask = (uint8_t)(~GPIOC->IDR & BUTTON_PINS_MASK); }

void bsp_buttons_init(void) { buttons_capture(); }

uint8_t bsp_buttons_get_pressed_mask(void) { return pressed_mask; }

void HAL_GPIO_EXTI_Callback(uint16_t pin) {
    if ((pin & BUTTON_PINS_MASK) != 0U) {
        buttons_capture();
    }
}
