#include "bsp_i2c.h"
#include "bsp_time.h"
#include "bsp_hw.h"
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

    /* bsp_board_init() prepares clocks, pins, DMA and NVIC.
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

void bsp_i2c_hw_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    GPIOB->MODER =
        (GPIOB->MODER & ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7)) | GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1;
    GPIOB->OTYPER |= GPIO_OTYPER_OT_6 | GPIO_OTYPER_OT_7;
    GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR6 | GPIO_OSPEEDR_OSPEEDR7;
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR6 | GPIO_PUPDR_PUPDR7);
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(GPIO_AFRL_AFSEL6 | GPIO_AFRL_AFSEL7)) | (1U << GPIO_AFRL_AFSEL6_Pos)
                    | (1U << GPIO_AFRL_AFSEL7_Pos);
    I2C1->CR1 = 0U;
    I2C1->CR2 = 0U;
    I2C1->OAR1 = 0U;
    I2C1->OAR2 = 0U;
    I2C1->TIMINGR = (2U << I2C_TIMINGR_SCLDEL_Pos) | (29U << I2C_TIMINGR_SCLH_Pos) | (43U << I2C_TIMINGR_SCLL_Pos);
    DMA1_Channel2->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PL_1;
    DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_PL_1;
    /* Analog filter enabled, digital filter off, clock stretching enabled. */
    I2C1->CR1 = I2C_CR1_PE;
    NVIC_SetPriority(I2C1_IRQn, 0U);
    NVIC_EnableIRQ(I2C1_IRQn);
}
