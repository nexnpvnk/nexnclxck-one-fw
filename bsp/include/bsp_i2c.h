#ifndef BSP_I2C_H
#define BSP_I2C_H
#include "ui2c.h"
#ifdef __cplusplus
extern "C" {
#endif
bool bsp_i2c_init(void);
ui2c_t* bsp_i2c_bus(void);
void bsp_i2c_process(void);
void bsp_i2c_irq(void);
void bsp_i2c_dma_irq(void);
#ifdef __cplusplus
}
#endif
#endif
