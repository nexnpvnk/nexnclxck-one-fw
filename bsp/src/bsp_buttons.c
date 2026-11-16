#include "bsp_buttons.h"

#include "stm32f0xx.h"

#define BUTTON_PINS_MASK (GPIO_IDR_0 | GPIO_IDR_1 | GPIO_IDR_2 | GPIO_IDR_3)

/**
 * \brief           Read the physical button levels directly from GPIOC.
 * \return          Mask of pressed buttons, indexed by `bsp_button_t`.
 */
uint8_t bsp_buttons_get_pressed_mask(void) {
    return (uint8_t)(~GPIOC->IDR & BUTTON_PINS_MASK);
}
