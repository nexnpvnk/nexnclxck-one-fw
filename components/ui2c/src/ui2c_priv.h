#ifndef UI2C_PRIV_H
#define UI2C_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui2c.h"

typedef enum {
    UI2C_STATE_UNINITIALIZED = 0,
    UI2C_STATE_IDLE,
    UI2C_STATE_ACTIVE,
    UI2C_STATE_COMPLETING,
} ui2c_state_t;

typedef struct {
    ui2c_status_t (*init)(ui2c_t* ui2c);
    ui2c_status_t (*start)(ui2c_t* ui2c);
    void (*process)(ui2c_t* ui2c);
    void (*abort)(ui2c_t* ui2c);
} ui2c_ops_t;

struct ui2c {
    const ui2c_ops_t* ops;
    void* driver_data;

    ui2c_config_t config;

    const ui2c_msg_t* msgs;
    size_t num_msgs;
    volatile size_t msg_idx;

    uint16_t addr;

    ui2c_callback_t callback;
    void* callback_arg;

    volatile ui2c_state_t state;
    volatile ui2c_status_t status;

    uint32_t start_time;
};

#define UI2C_DEFINE(name, ops_ptr, data_ptr) \
    ui2c_t name = {                         \
        .ops = (ops_ptr),                   \
        .driver_data = (data_ptr),          \
    }

void ui2c_core_complete(ui2c_t* ui2c, ui2c_status_t status);

const ui2c_msg_t* ui2c_core_current_msg(ui2c_t* ui2c);

size_t ui2c_core_current_msg_index(ui2c_t* ui2c);

bool ui2c_core_next_msg(ui2c_t* ui2c);

#endif /* UI2C_PRIV_H */
