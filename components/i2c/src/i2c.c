#include "i2c_priv.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool i2c_timeout_expired(i2c_t* i2c) {
    uint32_t now;
    uint32_t elapsed;

    if (i2c->config.timeout_ms == 0U) {
        return false;
    }

    if (i2c->config.time_ms == NULL) {
        return false;
    }

    now = i2c->config.time_ms(i2c->config.time_arg);

    /*
     * Unsigned subtraction intentionally handles uint32_t timer wraparound.
     */
    elapsed = now - i2c->start_time;

    return elapsed >= i2c->config.timeout_ms;
}

static bool i2c_msg_is_read(const i2c_msg_t* msg) { return (msg->flags & I2C_MSG_READ) != 0U; }

static i2c_status_t i2c_validate_msgs(const i2c_msg_t* msgs, size_t num_msgs) {
    size_t i;

    if ((msgs == NULL) || (num_msgs == 0U)) {
        return I2C_STATUS_INVALID_ARG;
    }

    for (i = 0U; i < num_msgs; ++i) {
        const i2c_msg_t* msg = &msgs[i];

        if ((msg->buf == NULL) && (msg->len != 0U)) {
            return I2C_STATUS_INVALID_ARG;
        }

        if ((i == 0U) && ((msg->flags & I2C_MSG_RESTART) != 0U)) {
            return I2C_STATUS_INVALID_ARG;
        }

        if ((i != (num_msgs - 1U)) && ((msg->flags & I2C_MSG_STOP) != 0U)) {
            return I2C_STATUS_INVALID_ARG;
        }

        if ((i > 0U) && (i2c_msg_is_read(&msgs[i - 1U]) != i2c_msg_is_read(msg))
            && ((msg->flags & I2C_MSG_RESTART) == 0U)) {
            return I2C_STATUS_INVALID_ARG;
        }
    }

    if ((msgs[num_msgs - 1U].flags & I2C_MSG_STOP) == 0U) {
        return I2C_STATUS_INVALID_ARG;
    }

    return I2C_STATUS_OK;
}

i2c_status_t i2c_init(i2c_t* i2c, const i2c_config_t* config) {
    i2c_status_t status;

    if ((i2c == NULL) || (config == NULL) || (i2c->ops == NULL) || (i2c->ops->init == NULL)
        || (i2c->ops->start == NULL)) {
        return I2C_STATUS_INVALID_ARG;
    }

    if ((config->timeout_ms != 0U) && (config->time_ms == NULL)) {
        return I2C_STATUS_INVALID_ARG;
    }

    i2c->config = *config;

    i2c->msgs = NULL;
    i2c->num_msgs = 0U;
    i2c->msg_idx = 0U;
    i2c->addr = 0U;

    i2c->callback = NULL;
    i2c->callback_arg = NULL;

    i2c->status = I2C_STATUS_OK;
    i2c->completion_pending = false;

    i2c->state = I2C_STATE_UNINITIALIZED;

    status = i2c->ops->init(i2c);
    if (status != I2C_STATUS_OK) {
        return status;
    }

    i2c->state = I2C_STATE_IDLE;

    return I2C_STATUS_OK;
}

i2c_status_t i2c_xfer(i2c_t* i2c, uint16_t addr, const i2c_msg_t* msgs, size_t num_msgs, i2c_callback_t callback,
                      void* arg) {
    i2c_status_t status;

    if (i2c == NULL) {
        return I2C_STATUS_INVALID_ARG;
    }

    if (i2c->state == I2C_STATE_UNINITIALIZED) {
        return I2C_STATUS_NOT_INITIALIZED;
    }

    if (i2c->state != I2C_STATE_IDLE) {
        return I2C_STATUS_BUSY;
    }

    if (addr > 0x7FU) {
        return I2C_STATUS_UNSUPPORTED;
    }

    status = i2c_validate_msgs(msgs, num_msgs);
    if (status != I2C_STATUS_OK) {
        return status;
    }

    i2c->msgs = msgs;
    i2c->num_msgs = num_msgs;
    i2c->msg_idx = 0U;
    i2c->addr = addr;

    i2c->callback = callback;
    i2c->callback_arg = arg;

    i2c->status = I2C_STATUS_OK;
    i2c->completion_pending = false;

    if (i2c->config.time_ms != NULL) {
        i2c->start_time = i2c->config.time_ms(i2c->config.time_arg);
    } else {
        i2c->start_time = 0U;
    }

    i2c->state = I2C_STATE_ACTIVE;

    status = i2c->ops->start(i2c);
    if (status != I2C_STATUS_OK) {
        i2c->state = I2C_STATE_IDLE;

        i2c->msgs = NULL;
        i2c->num_msgs = 0U;
        i2c->msg_idx = 0U;

        i2c->callback = NULL;
        i2c->callback_arg = NULL;

        return status;
    }

    return I2C_STATUS_OK;
}

void i2c_process(i2c_t* i2c) {
    i2c_callback_t callback;
    void* callback_arg;
    i2c_status_t status;

    if (i2c == NULL) {
        return;
    }

    if (i2c->state == I2C_STATE_ACTIVE) {
        if (i2c_timeout_expired(i2c)) {
            if (i2c->ops->abort != NULL) {
                i2c->ops->abort(i2c);
            }

            i2c_core_complete(i2c, I2C_STATUS_TIMEOUT);
        } else if (i2c->ops->process != NULL) {
            i2c->ops->process(i2c);
        }
    }

    if (!i2c->completion_pending) {
        return;
    }

    callback = i2c->callback;
    callback_arg = i2c->callback_arg;
    status = i2c->status;

    i2c->completion_pending = false;

    i2c->msgs = NULL;
    i2c->num_msgs = 0U;
    i2c->msg_idx = 0U;

    i2c->callback = NULL;
    i2c->callback_arg = NULL;

    i2c->state = I2C_STATE_IDLE;

    if (callback != NULL) {
        callback(i2c, status, callback_arg);
    }
}

void i2c_core_complete(i2c_t* i2c, i2c_status_t status) {
    if (i2c == NULL) {
        return;
    }

    if (i2c->state != I2C_STATE_ACTIVE) {
        return;
    }

    i2c->status = status;
    i2c->completion_pending = true;
    i2c->state = I2C_STATE_COMPLETING;
}

const i2c_msg_t* i2c_core_current_msg(i2c_t* i2c) {
    if ((i2c == NULL) || (i2c->msgs == NULL) || (i2c->msg_idx >= i2c->num_msgs)) {
        return NULL;
    }

    return &i2c->msgs[i2c->msg_idx];
}

size_t i2c_core_current_msg_index(i2c_t* i2c) {
    if (i2c == NULL) {
        return 0U;
    }

    return i2c->msg_idx;
}

bool i2c_core_next_msg(i2c_t* i2c) {
    if ((i2c == NULL) || ((i2c->msg_idx + 1U) >= i2c->num_msgs)) {
        return false;
    }

    ++i2c->msg_idx;

    return true;
}
