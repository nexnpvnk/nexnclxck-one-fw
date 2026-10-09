#ifndef I2C_PRIV_H
#define I2C_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "i2c.h"

typedef enum {
    I2C_STATE_UNINITIALIZED = 0,
    I2C_STATE_IDLE,
    I2C_STATE_ACTIVE,
    I2C_STATE_COMPLETING,
} i2c_state_t;

typedef struct {
    i2c_status_t (*init)(i2c_t* i2c);
    i2c_status_t (*start)(i2c_t* i2c);
    void (*process)(i2c_t* i2c);
    void (*abort)(i2c_t* i2c);
} i2c_ops_t;

struct i2c {
    const i2c_ops_t* ops;
    void* driver_data;

    i2c_config_t config;

    const i2c_msg_t* msgs;
    size_t num_msgs;
    size_t msg_idx;

    uint16_t addr;

    i2c_callback_t callback;
    void* callback_arg;

    i2c_state_t state;

    i2c_status_t status;

    uint32_t start_time;

    bool completion_pending;
};

#define I2C_DEFINE(name, ops_ptr, data_ptr)                                                                            \
    i2c_t name = {                                                                                                     \
        .ops = (ops_ptr),                                                                                              \
        .driver_data = (data_ptr),                                                                                     \
    }

void i2c_core_complete(i2c_t* i2c, i2c_status_t status);

const i2c_msg_t* i2c_core_current_msg(i2c_t* i2c);

size_t i2c_core_current_msg_index(i2c_t* i2c);

bool i2c_core_next_msg(i2c_t* i2c);

#endif /* I2C_PRIV_H */
