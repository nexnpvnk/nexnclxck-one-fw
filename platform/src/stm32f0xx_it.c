#include "stm32f0xx_it.h"
#include "bsp_console.h"
#include "bsp_hv.h"
#include "bsp_i2c.h"
#include "bsp_nixie.h"
#include "bsp_time.h"
#include "bsp_ws2812.h"
#include "main.h"

void NMI_Handler(void) {
    Error_Handler();
}

void HardFault_Handler(void) {
    Error_Handler();
}

void SVC_Handler(void) {}

void PendSV_Handler(void) {}

void SysTick_Handler(void) {
    bsp_time_tick();
}

void DMA1_Channel1_IRQHandler(void) {
    bsp_ws2812_dma_irq();
}

void DMA1_Channel2_3_IRQHandler(void) {
    bsp_i2c_dma_irq();
}

void DMA1_Channel4_5_IRQHandler(void) {
    bsp_console_dma_irq();
}

void ADC1_COMP_IRQHandler(void) {
    bsp_hv_adc_irq();
}

void TIM17_IRQHandler(void) {
    bsp_hv_pwm_irq();
}

void I2C1_IRQHandler(void) {
    bsp_i2c_irq();
}

void USART1_IRQHandler(void) {
    bsp_console_uart_irq();
}

void TIM1_CC_IRQHandler(void) {
    bsp_nixie_timer_irq();
}
