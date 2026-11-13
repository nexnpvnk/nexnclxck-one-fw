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

/* INS-1 separators are static outputs, never part of the multiplexed frame.
 * These channels are reserved even when the separators are off. */
#define BSP_SEPARATOR_ANODE_CHANNEL    2U
#define BSP_SEPARATOR_CATHODE_CHANNELS ((1U << 3U) | (1U << 7U) | (1U << 10U) | (1U << 13U))
#define BSP_SEPARATOR_ANODE_PORT       BSP_ANODE_2_PORT
#define BSP_SEPARATOR_ANODE_PIN        BSP_ANODE_2_PIN
#define BSP_SEPARATOR_CATHODE_B_MASK   (BSP_CATHODE_3_PIN | BSP_CATHODE_13_PIN)
#define BSP_SEPARATOR_CATHODE_C_MASK   (BSP_CATHODE_7_PIN | BSP_CATHODE_10_PIN)
#define BSP_NIXIE_SCAN_ANODE_B_MASK    (BSP_NIXIE_ANODE_B_MASK & ~BSP_SEPARATOR_ANODE_PIN)
#define BSP_NIXIE_SCAN_CATHODE_B_MASK  (BSP_NIXIE_CATHODE_B_MASK & ~BSP_SEPARATOR_CATHODE_B_MASK)
#define BSP_NIXIE_SCAN_CATHODE_C_MASK  (BSP_NIXIE_CATHODE_C_MASK & ~BSP_SEPARATOR_CATHODE_C_MASK)

#define BSP_BUTTON_RIGHT_PORT       GPIOC
#define BSP_BUTTON_RIGHT_PIN        GPIO_ODR_0
#define BSP_BUTTON_OK_PORT          GPIOC
#define BSP_BUTTON_OK_PIN           GPIO_ODR_1
#define BSP_BUTTON_LEFT_PORT        GPIOC
#define BSP_BUTTON_LEFT_PIN         GPIO_ODR_2
#define BSP_BUTTON_FN_PORT          GPIOC
#define BSP_BUTTON_FN_PIN           GPIO_ODR_3
/* LED anode is connected to +3.3 V; PC13 sinks current while LOW. */
#define BSP_RTC_LED_PORT            GPIOC
#define BSP_RTC_LED_PIN             GPIO_IDR_13
#define BSP_HV_ADC_PORT             GPIOA
#define BSP_HV_ADC_PIN              GPIO_ODR_2
#define BSP_I2C_SCL_BME280_PORT     GPIOB
#define BSP_I2C_SCL_BME280_PIN      GPIO_ODR_6
#define BSP_I2C_SDA_BME280_PORT     GPIOB
#define BSP_I2C_SDA_BME280_PIN      GPIO_ODR_7
#define BSP_ADDRESS_LED_DATA_0_PORT GPIOD
#define BSP_ADDRESS_LED_DATA_0_PIN  GPIO_ODR_2

#endif /* BSP_PINS_H */
