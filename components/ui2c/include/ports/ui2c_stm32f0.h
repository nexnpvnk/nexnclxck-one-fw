#ifndef UI2C_STM32F0_H
#define UI2C_STM32F0_H
#include "stm32f0xx.h"
#include "ui2c_port.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    DMA_Channel_TypeDef* channel;
    uint8_t number; /* 1-based DMA1 channel; routing is the board's responsibility. */
} ui2c_stm32f0_dma_t;

typedef struct {
    I2C_TypeDef* instance;
    ui2c_stm32f0_dma_t tx_dma; /* NULL channel disables DMA for this direction. */
    ui2c_stm32f0_dma_t rx_dma;
    uint16_t dma_threshold; /* 0 = default 16 bytes; IRQ fallback for short chunks. */
} ui2c_stm32f0_config_t;

/** Board-owned static storage. Zero-initialize, never copy after initialization.
 * Fields below are private runtime state; applications use &port.bus only. */
typedef struct {
    ui2c_t bus;
    ui2c_stm32f0_config_t hw;
    size_t index;
    size_t offset;
    uint16_t left;
    uint16_t dma_count;
    const ui2c_stm32f0_dma_t* dma;
    ui2c_boundary_t boundary;
    bool reload;
    ui2c_status_t pending_error;
} ui2c_stm32f0_t;

/** Board initializes clocks, pins, TIMINGR, filters and DMA routing first.
 * Configured DMA channels must be disabled, normal mode, byte/byte, MINC on,
 * PINC off, correct direction, MEM2MEM off. Board DMA priority is preserved.
 * Exclusively owns I2C transfers and configured DMA channels thereafter.
 * No HAL I2C transfers/callbacks may share the peripheral.
 * Board forwards I2C and DMA IRQs to the functions below; no global handlers.
 * NVIC may already be enabled, but peripheral IRQ/DMA requests must be inactive
 * until init completes. Existing timing and filter configuration is preserved. */
ui2c_status_t ui2c_stm32f0_init(ui2c_stm32f0_t* port, const ui2c_stm32f0_config_t* hw, const ui2c_config_t* config);
void ui2c_stm32f0_irq(ui2c_stm32f0_t* port);
void ui2c_stm32f0_dma_irq(ui2c_stm32f0_t* port);
#ifdef __cplusplus
}
#endif
#endif
