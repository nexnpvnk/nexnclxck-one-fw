#ifndef BSP_BUTTONS_H
#define BSP_BUTTONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_BUTTON_RIGHT = 0,
    BSP_BUTTON_OK,
    BSP_BUTTON_LEFT,
    BSP_BUTTON_LIGHT,
    BSP_BUTTON_COUNT,
} bsp_button_t;

/* Bit N corresponds to bsp_button_t value N.
 * Set bit means the button is physically pressed. */
void bsp_buttons_init(void);
uint8_t bsp_buttons_get_pressed_mask(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BUTTONS_H */
