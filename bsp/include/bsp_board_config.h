#ifndef BSP_BOARD_CONFIG_H
#define BSP_BOARD_CONFIG_H

/* Board device requirements. No MCU registers, clock tree or timer divisors. */
#define BSP_BOARD_BUZZER_INITIAL_RATE_HZ 2000UL
#define BSP_BOARD_HV_ADC_TARGET_RATE_HZ  2000UL
#define BSP_BOARD_HV_PWM_RATE_HZ         30000UL
#define BSP_BOARD_WS2812_BIT_RATE_HZ     800000UL
#define BSP_BOARD_WS2812_SYMBOLS_PER_BIT 3UL

#endif /* BSP_BOARD_CONFIG_H */
