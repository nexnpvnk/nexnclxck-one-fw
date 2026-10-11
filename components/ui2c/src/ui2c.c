#include "ui2c_port.h"

static ui2c_status_t validate(const ui2c_t* bus, const ui2c_msg_t* msgs, size_t count) {
    if (msgs == NULL || count == 0U) {
        return UI2C_STATUS_INVALID_ARG;
    }
    for (size_t i = 0; i < count; ++i) {
        const ui2c_msg_t* msg = &msgs[i];
        if ((msg->flags & ~(UI2C_MSG_READ | UI2C_MSG_RESTART)) != 0U
            || (i == 0U && (msg->flags & UI2C_MSG_RESTART) != 0U)) {
            return UI2C_STATUS_INVALID_ARG;
        }
        if (msg->len == 0U) {
            if (count != 1U || msg->flags != UI2C_MSG_WRITE) {
                return UI2C_STATUS_INVALID_ARG;
            }
            if ((bus->capabilities & UI2C_CAP_PROBE) == 0U) {
                return UI2C_STATUS_UNSUPPORTED;
            }
        } else if (((msg->flags & UI2C_MSG_READ) != 0U ? msg->data.rx : msg->data.tx) == NULL) {
            return UI2C_STATUS_INVALID_ARG;
        }
        if (i != 0U) {
            bool restart = (msg->flags & UI2C_MSG_RESTART) != 0U;
            if (!restart && ((msg->flags ^ msgs[i - 1U].flags) & UI2C_MSG_READ) != 0U) {
                return UI2C_STATUS_INVALID_ARG;
            }
            uint32_t required = restart ? UI2C_CAP_RESTART : UI2C_CAP_CONTINUE;
            if ((bus->capabilities & required) == 0U) {
                return UI2C_STATUS_UNSUPPORTED;
            }
        }
    }
    return UI2C_STATUS_OK;
}

ui2c_status_t ui2c_port_init(ui2c_t* bus, const ui2c_port_ops_t* ops, void* port, const ui2c_config_t* config,
                             uint32_t capabilities) {
    if (bus == NULL || ops == NULL || ops->lock == NULL || ops->unlock == NULL || ops->start == NULL
        || ops->abort == NULL || config == NULL || config->time_ms == NULL || config->timeout_ms == 0U
        || config->timeout_ms > INT32_MAX) {
        return UI2C_STATUS_INVALID_ARG;
    }
    if (bus->ops != NULL) {
        return UI2C_STATUS_BUSY;
    }
    bus->port = port;
    bus->config = *config;
    bus->capabilities = capabilities;
    bus->state = UI2C_IDLE;
    bus->ops = ops;
    return UI2C_STATUS_OK;
}

ui2c_status_t ui2c_xfer(ui2c_t* bus, uint16_t addr, const ui2c_msg_t* msgs, size_t count, ui2c_callback_t callback,
                        void* arg) {
    if (bus == NULL || callback == NULL) {
        return UI2C_STATUS_INVALID_ARG;
    }
    if (bus->ops == NULL) {
        return UI2C_STATUS_NOT_INITIALIZED;
    }
    if (addr > 0x7FU) {
        return UI2C_STATUS_UNSUPPORTED;
    }
    ui2c_status_t status = validate(bus, msgs, count);
    if (status != UI2C_STATUS_OK) {
        return status;
    }
    if (bus->ops->validate != NULL) {
        status = bus->ops->validate(bus, msgs, count);
        if (status != UI2C_STATUS_OK) {
            return status;
        }
    }
    uint32_t now = bus->config.time_ms(bus->config.time_arg);
    ui2c_lock_key_t key = bus->ops->lock(bus);
    if (bus->state != UI2C_IDLE) {
        bus->ops->unlock(bus, key);
        return UI2C_STATUS_BUSY;
    }
    bus->msgs = msgs;
    bus->num_msgs = count;
    bus->addr = addr;
    bus->callback = callback;
    bus->callback_arg = arg;
    bus->started_at = now;
    bus->result = UI2C_STATUS_OK;
    bus->state = UI2C_ACTIVE;
    status = bus->ops->start(bus);
    if (status != UI2C_STATUS_OK) {
        bus->state = UI2C_IDLE;
        bus->msgs = NULL;
        bus->num_msgs = 0U;
        bus->callback = NULL;
        bus->callback_arg = NULL;
    }
    bus->ops->unlock(bus, key);
    return status;
}

ui2c_status_t ui2c_cancel(ui2c_t* bus) {
    if (bus == NULL) {
        return UI2C_STATUS_INVALID_ARG;
    }
    if (bus->ops == NULL) {
        return UI2C_STATUS_NOT_INITIALIZED;
    }
    ui2c_lock_key_t key = bus->ops->lock(bus);
    if (bus->state == UI2C_ACTIVE) {
        bus->ops->abort(bus);
        ui2c_port_complete(bus, UI2C_STATUS_CANCELLED);
    }
    bus->ops->unlock(bus, key);
    return UI2C_STATUS_OK;
}

void ui2c_process(ui2c_t* bus) {
    if (bus == NULL || bus->ops == NULL) {
        return;
    }
    uint32_t now = bus->config.time_ms(bus->config.time_arg);
    ui2c_lock_key_t key = bus->ops->lock(bus);
    if (bus->state == UI2C_ACTIVE && (uint32_t)(now - bus->started_at) >= bus->config.timeout_ms) {
        bus->ops->abort(bus);
        ui2c_port_complete(bus, UI2C_STATUS_TIMEOUT);
    }
    if (bus->state != UI2C_DONE) {
        bus->ops->unlock(bus, key);
        return;
    }
    ui2c_callback_t callback = bus->callback;
    void* arg = bus->callback_arg;
    ui2c_status_t result = bus->result;
    bus->msgs = NULL;
    bus->num_msgs = 0U;
    bus->callback = NULL;
    bus->callback_arg = NULL;
    bus->state = UI2C_IDLE;
    bus->ops->unlock(bus, key);
    callback(bus, result, arg);
}

bool ui2c_busy(ui2c_t* bus) {
    if (bus == NULL || bus->ops == NULL) {
        return false;
    }
    ui2c_lock_key_t key = bus->ops->lock(bus);
    bool busy = bus->state != UI2C_IDLE;
    bus->ops->unlock(bus, key);
    return busy;
}

uint32_t ui2c_capabilities(const ui2c_t* bus) { return bus != NULL && bus->ops != NULL ? bus->capabilities : 0U; }

void ui2c_port_complete(ui2c_t* bus, ui2c_status_t status) {
    if (bus->state == UI2C_ACTIVE) {
        bus->result = status;
        bus->state = UI2C_DONE;
    }
}

ui2c_boundary_t ui2c_port_boundary(const ui2c_t* bus, size_t index) {
    if (index == bus->num_msgs - 1U) {
        return UI2C_BOUNDARY_STOP;
    }
    return (bus->msgs[index + 1U].flags & UI2C_MSG_RESTART) != 0U ? UI2C_BOUNDARY_RESTART : UI2C_BOUNDARY_CONTINUE;
}
