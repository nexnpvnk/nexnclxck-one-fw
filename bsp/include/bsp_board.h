#ifndef BSP_BOARD_H
#define BSP_BOARD_H

/* Boot-only preparation of this board's GPIO, IRQs and peripheral instances.
 * Call after SystemClock_Config(), with SysTick running, before app_main().
 * RTC/UART startup failures use the firmware's Error_Handler(). */
void bsp_board_init(void);

#endif /* BSP_BOARD_H */
