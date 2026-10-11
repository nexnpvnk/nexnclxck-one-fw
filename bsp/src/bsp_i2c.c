#include "bsp_i2c.h"
#include "i2c.h"
#include "ports/ui2c_stm32f0.h"
#include "stm32f0xx_hal.h"

static ui2c_stm32f0_t sensor_bus;

static uint32_t time_ms(void* arg) {
    (void)arg;
    return HAL_GetTick();
}

static ui2c_stm32f0_dma_t dma_config(const DMA_HandleTypeDef* handle) {
    ui2c_stm32f0_dma_t config = {0};
    if (handle == NULL) {
        return config; /* No DMA selected in CubeMX: use byte IRQs. */
    }
    DMA_Channel_TypeDef* const channels[] = {
        DMA1_Channel1, DMA1_Channel2, DMA1_Channel3, DMA1_Channel4, DMA1_Channel5,
    };
    config.channel = handle->Instance;
    for (uint8_t i = 0; i < 5; ++i) {
        if (channels[i] == config.channel) {
            config.number = i + 1U;
            break;
        }
    }
    return config;
}

bool bsp_i2c_init(void) {
    if (sensor_bus.bus.ops != NULL) {
        return true;
    }
    /* CubeMX owns all board initialization, including DMA and NVIC.
     * Bind its resources; the backend owns only transfer-time state. */
    const ui2c_stm32f0_config_t hw = {
        .instance = hi2c1.Instance,
        .tx_dma = dma_config(hi2c1.hdmatx),
        .rx_dma = dma_config(hi2c1.hdmarx),
        .dma_threshold = 16,
    };
    const ui2c_config_t config = {.time_ms = time_ms, .timeout_ms = 100};
    if (ui2c_stm32f0_init(&sensor_bus, &hw, &config) != UI2C_STATUS_OK) {
        return false;
    }
    return true;
}

ui2c_t* bsp_i2c_bus(void) { return &sensor_bus.bus; }

void bsp_i2c_process(void) { ui2c_process(&sensor_bus.bus); }

void bsp_i2c_irq(void) { ui2c_stm32f0_irq(&sensor_bus); }

void bsp_i2c_dma_irq(void) { ui2c_stm32f0_dma_irq(&sensor_bus); }
