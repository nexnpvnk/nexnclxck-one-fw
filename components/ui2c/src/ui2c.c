#include "ui2c_priv.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI2C_MSG_VALID_FLAGS \
    ((uint16_t)(UI2C_MSG_READ | UI2C_MSG_RESTART | UI2C_MSG_STOP))

static bool ui2c_timeout_expired(ui2c_t* ui2c) {
    uint32_t now;
    uint32_t elapsed;

    if (ui2c->config.timeout_ms == 0U) {
        return false;
    }

    if (ui2c->config.time_ms == NULL) {
        return false;
    }

    now = ui2c->config.time_ms(ui2c->config.time_arg);

    /*
     * Unsigned subtraction intentionally handles uint32_t timer wraparound.
     */
    elapsed = now - ui2c->start_time;

    return elapsed >= ui2c->config.timeout_ms;
}

static bool ui2c_msg_is_read(const ui2c_msg_t* msg) {
    return (msg->flags & UI2C_MSG_READ) != 0U;
}

static void ui2c_reset_xfer(ui2c_t* ui2c) {
    ui2c->msgs = NULL;
    ui2c->num_msgs = 0U;
    ui2c->msg_idx = 0U;
    ui2c->addr = 0U;

    ui2c->callback = NULL;
    ui2c->callback_arg = NULL;

    ui2c->status = UI2C_STATUS_OK;
}

static ui2c_status_t ui2c_validate_msgs(const ui2c_msg_t* msgs, size_t num_msgs) {
    size_t i;

    if ((msgs == NULL) || (num_msgs == 0U)) {
        return UI2C_STATUS_INVALID_ARG;
    }

    for (i = 0U; i < num_msgs; ++i) {
        const ui2c_msg_t* msg = &msgs[i];

        /*
         * Address-only transactions are deliberately not supported in v0.1.
         * They may be added later as an explicit capability.
         */
        if ((msg->buf == NULL) || (msg->len == 0U)) {
            return UI2C_STATUS_INVALID_ARG;
        }

        if ((msg->flags & ~UI2C_MSG_VALID_FLAGS) != 0U) {
            return UI2C_STATUS_INVALID_ARG;
        }

        /*
         * The first message always starts a new transaction,
         * therefore RESTART is meaningless on it.
         */
        if ((i == 0U) && ((msg->flags & UI2C_MSG_RESTART) != 0U)) {
            return UI2C_STATUS_INVALID_ARG;
        }

        /*
         * STOP terminates the complete transaction and is therefore
         * only allowed on the final message.
         */
        if ((i != (num_msgs - 1U)) && ((msg->flags & UI2C_MSG_STOP) != 0U)) {
            return UI2C_STATUS_INVALID_ARG;
        }

        /*
         * Changing direction requires a repeated START.
         */
        if ((i > 0U) && (ui2c_msg_is_read(&msgs[i - 1U]) != ui2c_msg_is_read(msg))
            && ((msg->flags & UI2C_MSG_RESTART) == 0U)) {
            return UI2C_STATUS_INVALID_ARG;
        }
    }

    /*
     * v0.1 transactions are self-contained and must always release the bus.
     */
    if ((msgs[num_msgs - 1U].flags & UI2C_MSG_STOP) == 0U) {
        return UI2C_STATUS_INVALID_ARG;
    }

    return UI2C_STATUS_OK;
}

ui2c_status_t ui2c_init(ui2c_t* ui2c, const ui2c_config_t* config) {
    ui2c_status_t status;

    if ((ui2c == NULL) || (config == NULL) || (config->frequency == 0U) || (ui2c->ops == NULL)
        || (ui2c->ops->init == NULL) || (ui2c->ops->start == NULL)) {
        return UI2C_STATUS_INVALID_ARG;
    }

    if ((ui2c->state == UI2C_STATE_ACTIVE) || (ui2c->state == UI2C_STATE_COMPLETING)) {
        return UI2C_STATUS_BUSY;
    }

    if ((config->timeout_ms != 0U) && (config->time_ms == NULL)) {
        return UI2C_STATUS_INVALID_ARG;
    }

    ui2c->config = *config;

    ui2c_reset_xfer(ui2c);

    ui2c->state = UI2C_STATE_UNINITIALIZED;

    status = ui2c->ops->init(ui2c);
    if (status != UI2C_STATUS_OK) {
        return status;
    }

    ui2c->state = UI2C_STATE_IDLE;

    return UI2C_STATUS_OK;
}

ui2c_status_t ui2c_xfer(ui2c_t* ui2c, uint16_t addr, const ui2c_msg_t* msgs, size_t num_msgs,
                        ui2c_callback_t callback, void* arg) {
    ui2c_status_t status;

    if (ui2c == NULL) {
        return UI2C_STATUS_INVALID_ARG;
    }

    if (ui2c->state == UI2C_STATE_UNINITIALIZED) {
        return UI2C_STATUS_NOT_INITIALIZED;
    }

    if (ui2c->state != UI2C_STATE_IDLE) {
        return UI2C_STATUS_BUSY;
    }

    /*
     * v0.1 supports 7-bit addressing only.
     */
    if (addr > 0x7FU) {
        return UI2C_STATUS_UNSUPPORTED;
    }

    status = ui2c_validate_msgs(msgs, num_msgs);
    if (status != UI2C_STATUS_OK) {
        return status;
    }

    ui2c->msgs = msgs;
    ui2c->num_msgs = num_msgs;
    ui2c->msg_idx = 0U;
    ui2c->addr = addr;

    ui2c->callback = callback;
    ui2c->callback_arg = arg;

    ui2c->status = UI2C_STATUS_OK;

    if (ui2c->config.time_ms != NULL) {
        ui2c->start_time = ui2c->config.time_ms(ui2c->config.time_arg);
    } else {
        ui2c->start_time = 0U;
    }

    ui2c->state = UI2C_STATE_ACTIVE;

    status = ui2c->ops->start(ui2c);
    if (status != UI2C_STATUS_OK) {
        ui2c_reset_xfer(ui2c);
        ui2c->state = UI2C_STATE_IDLE;

        return status;
    }

    return UI2C_STATUS_OK;
}

void ui2c_process(ui2c_t* ui2c) {
    ui2c_callback_t callback;
    void* callback_arg;
    ui2c_status_t status;

    if (ui2c == NULL) {
        return;
    }

    if (ui2c->state == UI2C_STATE_ACTIVE) {
        if (ui2c_timeout_expired(ui2c)) {
            if (ui2c->ops->abort != NULL) {
                ui2c->ops->abort(ui2c);
            }

            /*
             * The hardware ISR may have completed the transaction while
             * abort() was entering its critical section.
             *
             * Do not overwrite a real completion result in that case.
             */
            if (ui2c->state == UI2C_STATE_ACTIVE) {
                ui2c_core_complete(ui2c, UI2C_STATUS_TIMEOUT);
            }
        } else if (ui2c->ops->process != NULL) {
            ui2c->ops->process(ui2c);
        }
    }

    if (ui2c->state != UI2C_STATE_COMPLETING) {
        return;
    }

    callback = ui2c->callback;
    callback_arg = ui2c->callback_arg;
    status = ui2c->status;

    ui2c_reset_xfer(ui2c);

    /*
     * Set IDLE before calling application code so the callback may
     * immediately submit another transaction.
     */
    ui2c->state = UI2C_STATE_IDLE;

    if (callback != NULL) {
        callback(ui2c, status, callback_arg);
    }
}

void ui2c_core_complete(ui2c_t* ui2c, ui2c_status_t status) {
    if (ui2c == NULL) {
        return;
    }

    if (ui2c->state != UI2C_STATE_ACTIVE) {
        return;
    }

    /*
     * Store the result before publishing COMPLETING state.
     */
    ui2c->status = status;
    ui2c->state = UI2C_STATE_COMPLETING;
}

const ui2c_msg_t* ui2c_core_current_msg(ui2c_t* ui2c) {
    if ((ui2c == NULL) || (ui2c->msgs == NULL) || (ui2c->msg_idx >= ui2c->num_msgs)) {
        return NULL;
    }

    return &ui2c->msgs[ui2c->msg_idx];
}

size_t ui2c_core_current_msg_index(ui2c_t* ui2c) {
    if (ui2c == NULL) {
        return 0U;
    }

    return ui2c->msg_idx;
}

bool ui2c_core_next_msg(ui2c_t* ui2c) {
    if ((ui2c == NULL) || ((ui2c->msg_idx + 1U) >= ui2c->num_msgs)) {
        return false;
    }

    ++ui2c->msg_idx;

    return true;
}
