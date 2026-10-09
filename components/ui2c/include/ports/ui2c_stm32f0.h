#ifndef UI2C_PORT_STM32F0_H
#define UI2C_PORT_STM32F0_H

#include <stdint.h>

#include "ui2c.h"

#include "stm32f0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    I2C_HandleTypeDef* handle;
    IRQn_Type irqn;

    /*
     * Actual bus frequency configured by the board/CubeMX.
     *
     * The port uses it to verify that generic ui2c_config_t does not
     * request a frequency different from the hardware configuration.
     */
    uint32_t frequency;
} ui2c_stm32f0_config_t;

/*
 * Bind an STM32 HAL I2C handle to a statically allocated ui2c instance.
 *
 * No dynamic memory is used.
 */
ui2c_t* ui2c_stm32f0_bind(const ui2c_stm32f0_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* UI2C_PORT_STM32F0_H */
