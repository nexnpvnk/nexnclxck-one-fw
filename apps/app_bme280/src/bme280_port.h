#ifndef BME280_PORT_H
#define BME280_PORT_H
#include "bme280.h"
#include "ui2c.h"
#if !defined(BME280_64BIT_ENABLE) || defined(BME280_32BIT_ENABLE) || defined(BME280_DOUBLE_ENABLE)
#error "BME280 sample units require Bosch 64-bit integer compensation"
#endif

/* Transport context must outlive the Bosch device. Foreground only, with
 * interrupts and the bus timeout clock running; never call from a ui2c callback. */
typedef struct {
    ui2c_t* bus;
    uint8_t address;
    uint32_t (*time_ms)(void);
    ui2c_status_t status;
    bool done;
} bme280_port_t;

/* Installs only the Bosch read/write/delay hooks. Does not access the sensor. */
void bme280_port_bind(struct bme280_dev* dev, bme280_port_t* port);
#endif
