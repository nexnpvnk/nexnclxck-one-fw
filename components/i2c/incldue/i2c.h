#ifndef I2C_H
#define I2C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct i2c i2c_t;

typedef enum {
    I2C_STATUS_OK = 0,

    I2C_STATUS_INVALID_ARG,
    I2C_STATUS_NOT_INITIALIZED,
    I2C_STATUS_BUSY,
    I2C_STATUS_UNSUPPORTED,

    I2C_STATUS_NACK_ADDR,
    I2C_STATUS_NACK_DATA,

    I2C_STATUS_ARBITRATION_LOST,
    I2C_STATUS_BUS_BUSY,
    I2C_STATUS_BUS_ERROR,
    I2C_STATUS_TIMEOUT,

    I2C_STATUS_OVERRUN,
    I2C_STATUS_UNDERRUN,
    I2C_STATUS_DMA_ERROR,
    I2C_STATUS_IO_ERROR,
} i2c_status_t;

typedef enum {
    I2C_MSG_WRITE = 0x0000U,
    I2C_MSG_READ = 0x0001U,
    I2C_MSG_RESTART = 0x0002U,
    I2C_MSG_STOP = 0x0004U,
} i2c_msg_flags_t;

typedef struct {
    uint8_t* buf;
    size_t len;
    uint16_t flags;
} i2c_msg_t;

typedef uint32_t (*i2c_time_ms_t)(void* arg);

typedef void (*i2c_callback_t)(i2c_t* i2c, i2c_status_t status, void* arg);

typedef struct {
    uint32_t frequency;
    uint32_t timeout_ms;

    i2c_time_ms_t time_ms;
    void* time_arg;
} i2c_config_t;

i2c_status_t i2c_init(i2c_t* i2c, const i2c_config_t* config);

i2c_status_t i2c_xfer(i2c_t* i2c, uint16_t addr, const i2c_msg_t* msgs, size_t num_msgs, i2c_callback_t callback,
                      void* arg);

void i2c_process(i2c_t* i2c);

#ifdef __cplusplus
}
#endif

#endif /* I2C_H */
