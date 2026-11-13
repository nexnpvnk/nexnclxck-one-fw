#ifndef BSP_PINS_H
#define BSP_PINS_H

#include "stm32f0xx.h"

/* Physical carrier bindings; labels and numbering follow nexnclxck-one-fw.ioc.
 * Private to the hardware implementation. Anodes and cathodes are active HIGH. */
#define BSP_ANODE_0_PORT GPIOB
#define BSP_ANODE_0_PIN  GPIO_ODR_11
#define BSP_ANODE_1_PORT GPIOB
#define BSP_ANODE_1_PIN  GPIO_ODR_10
#define BSP_ANODE_2_PORT GPIOB
#define BSP_ANODE_2_PIN  GPIO_ODR_2
#define BSP_ANODE_3_PORT GPIOB
#define BSP_ANODE_3_PIN  GPIO_ODR_1
#define BSP_ANODE_4_PORT GPIOB
#define BSP_ANODE_4_PIN  GPIO_ODR_0
#define BSP_ANODE_5_PORT GPIOC
#define BSP_ANODE_5_PIN  GPIO_ODR_5
#define BSP_ANODE_6_PORT GPIOC
#define BSP_ANODE_6_PIN  GPIO_ODR_4
#define BSP_ANODE_7_PORT GPIOA
#define BSP_ANODE_7_PIN  GPIO_ODR_5

#define BSP_NIXIE_ANODE_BINDINGS                                                                                       \
    {BSP_ANODE_0_PORT, BSP_ANODE_0_PIN}, {BSP_ANODE_1_PORT, BSP_ANODE_1_PIN}, {BSP_ANODE_2_PORT, BSP_ANODE_2_PIN},     \
        {BSP_ANODE_3_PORT, BSP_ANODE_3_PIN}, {BSP_ANODE_4_PORT, BSP_ANODE_4_PIN}, {BSP_ANODE_5_PORT, BSP_ANODE_5_PIN}, \
        {BSP_ANODE_6_PORT, BSP_ANODE_6_PIN}, {                                                                         \
        BSP_ANODE_7_PORT, BSP_ANODE_7_PIN                                                                              \
    }

#define BSP_NIXIE_ANODE_A_MASK (BSP_ANODE_7_PIN)
#define BSP_NIXIE_ANODE_B_MASK (BSP_ANODE_0_PIN | BSP_ANODE_1_PIN | BSP_ANODE_2_PIN | BSP_ANODE_3_PIN | BSP_ANODE_4_PIN)
#define BSP_NIXIE_ANODE_C_MASK (BSP_ANODE_5_PIN | BSP_ANODE_6_PIN)

#define BSP_CATHODE_0_PORT     GPIOB
#define BSP_CATHODE_0_PIN      GPIO_ODR_9
#define BSP_CATHODE_1_PORT     GPIOB
#define BSP_CATHODE_1_PIN      GPIO_ODR_8
#define BSP_CATHODE_2_PORT     GPIOB
#define BSP_CATHODE_2_PIN      GPIO_ODR_5
#define BSP_CATHODE_3_PORT     GPIOB
#define BSP_CATHODE_3_PIN      GPIO_ODR_4
#define BSP_CATHODE_4_PORT     GPIOB
#define BSP_CATHODE_4_PIN      GPIO_ODR_3
#define BSP_CATHODE_5_PORT     GPIOC
#define BSP_CATHODE_5_PIN      GPIO_ODR_12
#define BSP_CATHODE_6_PORT     GPIOC
#define BSP_CATHODE_6_PIN      GPIO_ODR_11
#define BSP_CATHODE_7_PORT     GPIOC
#define BSP_CATHODE_7_PIN      GPIO_ODR_10
#define BSP_CATHODE_8_PORT     GPIOC
#define BSP_CATHODE_8_PIN      GPIO_ODR_9
#define BSP_CATHODE_9_PORT     GPIOC
#define BSP_CATHODE_9_PIN      GPIO_ODR_8
#define BSP_CATHODE_10_PORT    GPIOC
#define BSP_CATHODE_10_PIN     GPIO_ODR_7
#define BSP_CATHODE_11_PORT    GPIOC
#define BSP_CATHODE_11_PIN     GPIO_ODR_6
#define BSP_CATHODE_12_PORT    GPIOB
#define BSP_CATHODE_12_PIN     GPIO_ODR_15
#define BSP_CATHODE_13_PORT    GPIOB
#define BSP_CATHODE_13_PIN     GPIO_ODR_14
#define BSP_CATHODE_14_PORT    GPIOB
#define BSP_CATHODE_14_PIN     GPIO_ODR_13
#define BSP_CATHODE_15_PORT    GPIOB
#define BSP_CATHODE_15_PIN     GPIO_ODR_12

#define BSP_NIXIE_CATHODE_BINDINGS                                                                                     \
    {BSP_CATHODE_0_PORT, BSP_CATHODE_0_PIN}, {BSP_CATHODE_1_PORT, BSP_CATHODE_1_PIN},                                  \
        {BSP_CATHODE_2_PORT, BSP_CATHODE_2_PIN}, {BSP_CATHODE_3_PORT, BSP_CATHODE_3_PIN},                              \
        {BSP_CATHODE_4_PORT, BSP_CATHODE_4_PIN}, {BSP_CATHODE_5_PORT, BSP_CATHODE_5_PIN},                              \
        {BSP_CATHODE_6_PORT, BSP_CATHODE_6_PIN}, {BSP_CATHODE_7_PORT, BSP_CATHODE_7_PIN},                              \
        {BSP_CATHODE_8_PORT, BSP_CATHODE_8_PIN}, {BSP_CATHODE_9_PORT, BSP_CATHODE_9_PIN},                              \
        {BSP_CATHODE_10_PORT, BSP_CATHODE_10_PIN}, {BSP_CATHODE_11_PORT, BSP_CATHODE_11_PIN},                          \
        {BSP_CATHODE_12_PORT, BSP_CATHODE_12_PIN}, {BSP_CATHODE_13_PORT, BSP_CATHODE_13_PIN},                          \
        {BSP_CATHODE_14_PORT, BSP_CATHODE_14_PIN}, {                                                                   \
        BSP_CATHODE_15_PORT, BSP_CATHODE_15_PIN                                                                        \
    }

#define BSP_NIXIE_CATHODE_A_MASK (0U)
#define BSP_NIXIE_CATHODE_B_MASK                                                                                       \
    (BSP_CATHODE_0_PIN | BSP_CATHODE_1_PIN | BSP_CATHODE_2_PIN | BSP_CATHODE_3_PIN | BSP_CATHODE_4_PIN                 \
     | BSP_CATHODE_12_PIN | BSP_CATHODE_13_PIN | BSP_CATHODE_14_PIN | BSP_CATHODE_15_PIN)
#define BSP_NIXIE_CATHODE_C_MASK                                                                                       \
    (BSP_CATHODE_5_PIN | BSP_CATHODE_6_PIN | BSP_CATHODE_7_PIN | BSP_CATHODE_8_PIN | BSP_CATHODE_9_PIN                 \
     | BSP_CATHODE_10_PIN | BSP_CATHODE_11_PIN)

/* LED anode is connected to +3.3 V; PC13 sinks current while LOW. */
#define BSP_RTC_LED_PORT            GPIOC
#define BSP_RTC_LED_PIN             GPIO_IDR_13

#endif /* BSP_PINS_H */
