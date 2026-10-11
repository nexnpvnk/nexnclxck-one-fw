#include "bme280_port.h"

static void completed(ui2c_t* bus, ui2c_status_t status, void* arg) {
    (void)bus;
    bme280_port_t* port = arg;
    port->status = status;
    port->done = true;
}

static BME280_INTF_RET_TYPE transfer(bme280_port_t* port, const ui2c_msg_t* msgs) {
    port->done = false;
    port->status = ui2c_xfer(port->bus, port->address, msgs, 2, completed, port);
    if (port->status == UI2C_STATUS_OK) {
        /* Also advances polling backends and enforces the ui2c timeout.
         * Keep stack descriptors/buffers alive until completion is delivered. */
        while (!port->done) {
            ui2c_process(port->bus);
        }
    }
    return port->status == UI2C_STATUS_OK ? BME280_INTF_RET_SUCCESS : -1;
}

static BME280_INTF_RET_TYPE read_regs(uint8_t reg, uint8_t* data, uint32_t len, void* arg) {
    const ui2c_msg_t msgs[] = {
        {.data.tx = &reg, .len = 1, .flags = UI2C_MSG_WRITE},
        {.data.rx = data, .len = len, .flags = UI2C_MSG_READ | UI2C_MSG_RESTART},
    };
    return transfer(arg, msgs);
}

static BME280_INTF_RET_TYPE write_regs(uint8_t reg, const uint8_t* data, uint32_t len, void* arg) {
    const ui2c_msg_t msgs[] = {
        {.data.tx = &reg, .len = 1, .flags = UI2C_MSG_WRITE},
        {.data.tx = data, .len = len, .flags = UI2C_MSG_WRITE},
    };
    return transfer(arg, msgs);
}

static void delay_us(uint32_t period, void* arg) {
    bme280_port_t* port = arg;
    /* Extra tick covers the unknown phase of the millisecond clock. */
    uint32_t duration = period / 1000U + (period % 1000U != 0U) + 1U;
    uint32_t start = port->time_ms();
    while ((uint32_t)(port->time_ms() - start) < duration) {}
}

void bme280_port_bind(struct bme280_dev* dev, bme280_port_t* port) {
    *dev = (struct bme280_dev){
        .intf = BME280_I2C_INTF,
        .intf_ptr = port,
        .read = read_regs,
        .write = write_regs,
        .delay_us = delay_us,
    };
}
