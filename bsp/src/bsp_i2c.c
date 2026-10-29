#include "bsp_i2c.h"
#include "bsp_time.h"
#include "i2c.h"
#include "ui2c_stm32f0.h"

#define BSP_I2C_INIT_RETRY_MS 1000U

static ui2c_stm32f0_t sensor_bus;
static uint32_t last_init_attempt_ms;

static uint32_t time_ms(void* arg) {
    (void)arg;
    return bsp_time_ms();
}

bool bsp_i2c_init(void) {
    if (sensor_bus.bus.ops != NULL) {
        return true;
    }

    last_init_attempt_ms = bsp_time_ms();

    /* Core initialization owns clocks, pins, DMA and NVIC.
     * The backend owns only transfer-time state. */
    const ui2c_stm32f0_config_t hw = {
        .instance = I2C1,
        .tx_dma = {DMA1_Channel2, 2},
        .rx_dma = {DMA1_Channel3, 3},
        .dma_threshold = 16,
    };

    const ui2c_config_t config = {
        .time_ms = time_ms,
        .timeout_ms = 100,
    };

    return ui2c_stm32f0_init(&sensor_bus, &hw, &config) == UI2C_STATUS_OK;
}

ui2c_t* bsp_i2c_bus(void) { return &sensor_bus.bus; }

void bsp_i2c_process(void) {
    if (sensor_bus.bus.ops != NULL) {
        ui2c_process(&sensor_bus.bus);
        return;
    }

    if ((uint32_t)(bsp_time_ms() - last_init_attempt_ms) >= BSP_I2C_INIT_RETRY_MS) {
        (void)bsp_i2c_init();
    }
}

void bsp_i2c_irq(void) { ui2c_stm32f0_irq(&sensor_bus); }

void bsp_i2c_dma_irq(void) { ui2c_stm32f0_dma_irq(&sensor_bus); }
