#include "ui2c.h"
#include "ports/ui2c_stm32f0.h"

#include "ui2c_priv.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef UI2C_STM32F0_MAX_INSTANCES
#define UI2C_STM32F0_MAX_INSTANCES 2U
#endif

typedef struct {
    I2C_HandleTypeDef* handle;
    IRQn_Type irqn;

    uint32_t frequency;

    size_t msg_offset;
    size_t chunk_len;
} ui2c_stm32f0_data_t;

typedef struct {
    bool used;

    ui2c_t ui2c;
    ui2c_stm32f0_data_t data;
} ui2c_stm32f0_slot_t;

static ui2c_status_t ui2c_stm32f0_init(ui2c_t* ui2c);
static ui2c_status_t ui2c_stm32f0_start(ui2c_t* ui2c);
static void ui2c_stm32f0_abort(ui2c_t* ui2c);

static const ui2c_ops_t ui2c_stm32f0_ops = {
    .init = ui2c_stm32f0_init,
    .start = ui2c_stm32f0_start,
    .process = NULL,
    .abort = ui2c_stm32f0_abort,
};

static ui2c_stm32f0_slot_t ui2c_stm32f0_slots[UI2C_STM32F0_MAX_INSTANCES];

static ui2c_stm32f0_data_t* ui2c_stm32f0_data(ui2c_t* ui2c) {
    return (ui2c_stm32f0_data_t*)ui2c->driver_data;
}

static ui2c_t* ui2c_stm32f0_find(I2C_HandleTypeDef* handle) {
    size_t i;

    for (i = 0U; i < UI2C_STM32F0_MAX_INSTANCES; ++i) {
        if (!ui2c_stm32f0_slots[i].used) {
            continue;
        }

        if (ui2c_stm32f0_slots[i].data.handle == handle) {
            return &ui2c_stm32f0_slots[i].ui2c;
        }
    }

    return NULL;
}

static ui2c_status_t ui2c_stm32f0_error_to_status(uint32_t error) {
    if ((error & HAL_I2C_ERROR_ARLO) != 0U) {
        return UI2C_STATUS_ARBITRATION_LOST;
    }

    if ((error & HAL_I2C_ERROR_BERR) != 0U) {
        return UI2C_STATUS_BUS_ERROR;
    }

    if ((error & HAL_I2C_ERROR_TIMEOUT) != 0U) {
        return UI2C_STATUS_TIMEOUT;
    }

    if ((error & HAL_I2C_ERROR_AF) != 0U) {
        /*
         * STM32 HAL exposes ACK failure, but does not reliably provide
         * enough public information to distinguish address NACK from
         * data NACK in every transfer mode.
         */
        return UI2C_STATUS_NACK;
    }

    if ((error & HAL_I2C_ERROR_OVR) != 0U) {
        /*
         * STM32 HAL combines overrun/underrun into HAL_I2C_ERROR_OVR.
         */
        return UI2C_STATUS_OVERRUN;
    }

    if ((error & (HAL_I2C_ERROR_DMA | HAL_I2C_ERROR_DMA_PARAM)) != 0U) {
        return UI2C_STATUS_DMA_ERROR;
    }

    if ((error & HAL_I2C_ERROR_INVALID_PARAM) != 0U) {
        return UI2C_STATUS_INVALID_ARG;
    }

    if ((error & HAL_I2C_ERROR_SIZE) != 0U) {
        return UI2C_STATUS_UNSUPPORTED;
    }

    return UI2C_STATUS_IO_ERROR;
}

static ui2c_status_t ui2c_stm32f0_hal_status(I2C_HandleTypeDef* handle, HAL_StatusTypeDef status) {
    switch (status) {
        case HAL_OK:
            return UI2C_STATUS_OK;

        case HAL_BUSY:
            return UI2C_STATUS_BUSY;

        case HAL_TIMEOUT:
            return UI2C_STATUS_TIMEOUT;

        case HAL_ERROR:
        default:
            return ui2c_stm32f0_error_to_status(HAL_I2C_GetError(handle));
    }
}

static uint32_t ui2c_stm32f0_xfer_options(ui2c_t* ui2c, const ui2c_msg_t* msg, bool last_chunk) {
    ui2c_stm32f0_data_t* data;
    size_t msg_idx;

    bool first_msg;
    bool first_chunk;
    bool last_msg;
    bool restart;

    data = ui2c_stm32f0_data(ui2c);

    msg_idx = ui2c_core_current_msg_index(ui2c);

    first_msg = msg_idx == 0U;
    first_chunk = data->msg_offset == 0U;
    last_msg = (msg_idx + 1U) == ui2c->num_msgs;
    restart = (msg->flags & UI2C_MSG_RESTART) != 0U;

    /*
     * Complete one-message transaction:
     *
     * START ... STOP
     */
    if (first_msg && first_chunk && last_msg && last_chunk) {
        return I2C_FIRST_AND_LAST_FRAME;
    }

    /*
     * First frame of a multi-message/chunk transaction:
     *
     * START ... no STOP
     */
    if (first_msg && first_chunk) {
        return I2C_FIRST_FRAME;
    }

    /*
     * Explicit repeated START, including the same direction.
     */
    if (first_chunk && restart) {
        if (last_msg && last_chunk) {
            return I2C_OTHER_AND_LAST_FRAME;
        }

        return I2C_OTHER_FRAME;
    }

    /*
     * Final continuation frame:
     *
     * no START ... STOP
     */
    if (last_msg && last_chunk) {
        return I2C_LAST_FRAME;
    }

    /*
     * Continuation without START and without STOP.
     */
    return I2C_NEXT_FRAME;
}

static ui2c_status_t ui2c_stm32f0_start_chunk(ui2c_t* ui2c) {
    ui2c_stm32f0_data_t* data;
    const ui2c_msg_t* msg;

    HAL_StatusTypeDef hal_status;

    uint32_t xfer_options;

    uint16_t address;
    uint16_t chunk_len;

    size_t remaining;

    bool last_chunk;

    uint8_t* buf;

    data = ui2c_stm32f0_data(ui2c);
    msg = ui2c_core_current_msg(ui2c);

    if ((data == NULL) || (data->handle == NULL) || (msg == NULL)) {
        return UI2C_STATUS_IO_ERROR;
    }

    if (data->msg_offset >= msg->len) {
        return UI2C_STATUS_IO_ERROR;
    }

    remaining = msg->len - data->msg_offset;

    /*
     * STM32 HAL exposes Size as uint16_t.
     *
     * Its internal I2C v2 implementation already handles the 255-byte
     * NBYTES hardware limit using RELOAD/TCR. We only need to split
     * messages larger than HAL's uint16_t API limit.
     */
    if (remaining > UINT16_MAX) {
        chunk_len = UINT16_MAX;
        last_chunk = false;
    } else {
        chunk_len = (uint16_t)remaining;
        last_chunk = true;
    }

    data->chunk_len = chunk_len;

    buf = &msg->buf[data->msg_offset];

    xfer_options = ui2c_stm32f0_xfer_options(ui2c, msg, last_chunk);

    /*
     * STM32 HAL expects a 7-bit address shifted one position left.
     */
    address = (uint16_t)(ui2c->addr << 1U);

    if ((msg->flags & UI2C_MSG_READ) != 0U) {
        hal_status =
            HAL_I2C_Master_Seq_Receive_IT(data->handle, address, buf, chunk_len, xfer_options);
    } else {
        hal_status =
            HAL_I2C_Master_Seq_Transmit_IT(data->handle, address, buf, chunk_len, xfer_options);
    }

    return ui2c_stm32f0_hal_status(data->handle, hal_status);
}

static void ui2c_stm32f0_abort(ui2c_t* ui2c) {
    ui2c_stm32f0_data_t* data;
    I2C_HandleTypeDef* handle;

    data = ui2c_stm32f0_data(ui2c);

    if ((data == NULL) || (data->handle == NULL)) {
        return;
    }

    handle = data->handle;

    /*
     * Disable this peripheral IRQ first.
     *
     * If the transfer completed immediately before the IRQ was disabled,
     * the callback has already moved ui2c to COMPLETING and we must not
     * destroy that valid result.
     */
    HAL_NVIC_DisableIRQ(data->irqn);

    if (ui2c->state != UI2C_STATE_ACTIVE) {
        HAL_NVIC_EnableIRQ(data->irqn);
        return;
    }

    /*
     * Stop local interrupt/DMA requests.
     *
     * We intentionally do not generate a STOP condition here. On a
     * multi-controller bus we may no longer own the bus.
     */
    CLEAR_BIT(handle->Instance->CR1,
              I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_ADDRIE | I2C_CR1_NACKIE | I2C_CR1_STOPIE | I2C_CR1_TCIE
                  | I2C_CR1_ERRIE | I2C_CR1_TXDMAEN | I2C_CR1_RXDMAEN);

    /*
     * Disable only our local peripheral. This releases its control over
     * SDA/SCL without attempting to recover or otherwise modify the
     * physical bus.
     */
    __HAL_I2C_DISABLE(handle);

    /*
     * Clear sticky status flags belonging to this peripheral instance.
     */
    __HAL_I2C_CLEAR_FLAG(handle, I2C_FLAG_BERR);
    __HAL_I2C_CLEAR_FLAG(handle, I2C_FLAG_ARLO);
    __HAL_I2C_CLEAR_FLAG(handle, I2C_FLAG_OVR);
    __HAL_I2C_CLEAR_FLAG(handle, I2C_FLAG_AF);
    __HAL_I2C_CLEAR_FLAG(handle, I2C_FLAG_STOPF);

    /*
     * Reset runtime transfer configuration.
     *
     * v0.1 supports 7-bit controller mode only, therefore ADD10 and
     * other transaction-specific CR2 fields are deliberately cleared.
     *
     * AUTOEND + NACK is the idle configuration used by STM32 HAL.
     */
    WRITE_REG(handle->Instance->CR2, I2C_CR2_AUTOEND | I2C_CR2_NACK);

    /*
     * Restore the public HAL handle to an idle state.
     *
     * We avoid HAL_I2C_DeInit()/HAL_I2C_Init() here because doing so
     * would unnecessarily destroy/recreate GPIO and board-specific
     * filter configuration.
     */
    handle->pBuffPtr = NULL;
    handle->XferSize = 0U;
    handle->XferCount = 0U;
    handle->XferOptions = 0U;
    handle->PreviousState = 0U;
    handle->XferISR = NULL;

    handle->ErrorCode = HAL_I2C_ERROR_NONE;
    handle->Mode = HAL_I2C_MODE_NONE;
    handle->State = HAL_I2C_STATE_READY;
    handle->Lock = HAL_UNLOCKED;

    data->msg_offset = 0U;
    data->chunk_len = 0U;

    __HAL_I2C_ENABLE(handle);

    HAL_NVIC_ClearPendingIRQ(data->irqn);
    HAL_NVIC_EnableIRQ(data->irqn);
}

static void ui2c_stm32f0_fail(ui2c_t* ui2c, ui2c_status_t status) {
    ui2c_stm32f0_abort(ui2c);

    if (ui2c->state == UI2C_STATE_ACTIVE) {
        ui2c_core_complete(ui2c, status);
    }
}

static void ui2c_stm32f0_frame_complete(ui2c_t* ui2c) {
    ui2c_stm32f0_data_t* data;
    const ui2c_msg_t* msg;

    ui2c_status_t status;

    if (ui2c == NULL) {
        return;
    }

    if (ui2c->state != UI2C_STATE_ACTIVE) {
        return;
    }

    data = ui2c_stm32f0_data(ui2c);

    if (data == NULL) {
        ui2c_core_complete(ui2c, UI2C_STATUS_IO_ERROR);
        return;
    }

    msg = ui2c_core_current_msg(ui2c);
    if (msg == NULL) {
        ui2c_core_complete(ui2c, UI2C_STATUS_IO_ERROR);
        return;
    }

    data->msg_offset += data->chunk_len;

    /*
     * Continue the same generic message if its size exceeded the
     * uint16_t STM32 HAL API limit.
     */
    if (data->msg_offset < msg->len) {
        status = ui2c_stm32f0_start_chunk(ui2c);

        if (status != UI2C_STATUS_OK) {
            ui2c_stm32f0_fail(ui2c, status);
        }

        return;
    }

    data->msg_offset = 0U;
    data->chunk_len = 0U;

    /*
     * Continue with the next message in the same atomic transaction.
     */
    if (ui2c_core_next_msg(ui2c)) {
        status = ui2c_stm32f0_start_chunk(ui2c);

        if (status != UI2C_STATUS_OK) {
            ui2c_stm32f0_fail(ui2c, status);
        }

        return;
    }

    ui2c_core_complete(ui2c, UI2C_STATUS_OK);
}

static ui2c_status_t ui2c_stm32f0_init(ui2c_t* ui2c) {
    ui2c_stm32f0_data_t* data;
    HAL_I2C_StateTypeDef state;

    data = ui2c_stm32f0_data(ui2c);

    if ((data == NULL) || (data->handle == NULL) || (data->handle->Instance == NULL)) {
        return UI2C_STATUS_INVALID_ARG;
    }

    /*
     * CubeMX owns TIMINGR configuration in this project.
     *
     * Do not silently accept a generic frequency different from the
     * actual board configuration.
     */
    if (ui2c->config.frequency != data->frequency) {
        return UI2C_STATUS_UNSUPPORTED;
    }

    if (data->handle->Init.AddressingMode != I2C_ADDRESSINGMODE_7BIT) {
        return UI2C_STATUS_UNSUPPORTED;
    }

    state = HAL_I2C_GetState(data->handle);

    if (state == HAL_I2C_STATE_RESET) {
        return UI2C_STATUS_NOT_INITIALIZED;
    }

    if (state != HAL_I2C_STATE_READY) {
        return UI2C_STATUS_BUSY;
    }

    data->msg_offset = 0U;
    data->chunk_len = 0U;

    /*
     * The port is interrupt-driven. CubeMX configures the priority;
     * enabling it here guarantees that ui2c itself is operational.
     */
    HAL_NVIC_EnableIRQ(data->irqn);

    return UI2C_STATUS_OK;
}

static ui2c_status_t ui2c_stm32f0_start(ui2c_t* ui2c) {
    ui2c_stm32f0_data_t* data;

    data = ui2c_stm32f0_data(ui2c);

    if ((data == NULL) || (data->handle == NULL)) {
        return UI2C_STATUS_INVALID_ARG;
    }

    if (HAL_I2C_GetState(data->handle) != HAL_I2C_STATE_READY) {
        return UI2C_STATUS_BUSY;
    }

    data->msg_offset = 0U;
    data->chunk_len = 0U;

    return ui2c_stm32f0_start_chunk(ui2c);
}

ui2c_t* ui2c_stm32f0_bind(const ui2c_stm32f0_config_t* config) {
    ui2c_stm32f0_slot_t* slot;
    size_t i;

    if ((config == NULL) || (config->handle == NULL) || (config->frequency == 0U)) {
        return NULL;
    }

    /*
     * Return the existing binding when the same HAL instance is bound
     * more than once.
     */
    for (i = 0U; i < UI2C_STM32F0_MAX_INSTANCES; ++i) {
        slot = &ui2c_stm32f0_slots[i];

        if (!slot->used) {
            continue;
        }

        if (slot->data.handle != config->handle) {
            continue;
        }

        if ((slot->data.frequency != config->frequency) || (slot->data.irqn != config->irqn)) {
            return NULL;
        }

        return &slot->ui2c;
    }

    /*
     * Allocate from the fixed static port-instance table.
     */
    for (i = 0U; i < UI2C_STM32F0_MAX_INSTANCES; ++i) {
        slot = &ui2c_stm32f0_slots[i];

        if (slot->used) {
            continue;
        }

        slot->used = true;

        slot->data.handle = config->handle;
        slot->data.irqn = config->irqn;
        slot->data.frequency = config->frequency;

        slot->data.msg_offset = 0U;
        slot->data.chunk_len = 0U;

        slot->ui2c.ops = &ui2c_stm32f0_ops;
        slot->ui2c.driver_data = &slot->data;
        slot->ui2c.state = UI2C_STATE_UNINITIALIZED;

        return &slot->ui2c;
    }

    return NULL;
}

/*
 * STM32 HAL invokes these callbacks from I2C IRQ context.
 *
 * Hardware sequencing is continued immediately here because delaying the
 * next frame until ui2c_process() could hold the bus unnecessarily.
 *
 * The application callback itself remains deferred and is only invoked
 * from ui2c_process().
 */

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef* handle) {
    ui2c_t* ui2c;

    ui2c = ui2c_stm32f0_find(handle);

    if (ui2c != NULL) {
        ui2c_stm32f0_frame_complete(ui2c);
    }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef* handle) {
    ui2c_t* ui2c;

    ui2c = ui2c_stm32f0_find(handle);

    if (ui2c != NULL) {
        ui2c_stm32f0_frame_complete(ui2c);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* handle) {
    ui2c_t* ui2c;

    ui2c = ui2c_stm32f0_find(handle);
    if (ui2c == NULL) {
        return;
    }

    if (ui2c->state != UI2C_STATE_ACTIVE) {
        return;
    }

    ui2c_core_complete(ui2c, ui2c_stm32f0_error_to_status(HAL_I2C_GetError(handle)));
}
