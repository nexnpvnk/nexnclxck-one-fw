#ifndef UI2C_UI2C_H
#define UI2C_UI2C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui2c ui2c_t;

typedef enum {
    UI2C_STATUS_OK = 0,

    UI2C_STATUS_INVALID_ARG,
    UI2C_STATUS_NOT_INITIALIZED,
    UI2C_STATUS_BUSY,
    UI2C_STATUS_UNSUPPORTED,

    UI2C_STATUS_NACK,
    UI2C_STATUS_NACK_ADDR,
    UI2C_STATUS_NACK_DATA,

    UI2C_STATUS_ARBITRATION_LOST,
    UI2C_STATUS_BUS_BUSY,
    UI2C_STATUS_BUS_ERROR,
    UI2C_STATUS_TIMEOUT,

    UI2C_STATUS_OVERRUN,
    UI2C_STATUS_UNDERRUN,
    UI2C_STATUS_DMA_ERROR,
    UI2C_STATUS_IO_ERROR,
} ui2c_status_t;

typedef enum {
    UI2C_MSG_WRITE = 0x0000U,
    UI2C_MSG_READ = 0x0001U,
    UI2C_MSG_RESTART = 0x0002U,
    UI2C_MSG_STOP = 0x0004U,
} ui2c_msg_flags_t;

typedef struct {
    uint8_t* buf;
    size_t len;
    uint16_t flags;
} ui2c_msg_t;

typedef uint32_t (*ui2c_time_ms_t)(void* arg);

typedef void (*ui2c_callback_t)(ui2c_t* ui2c, ui2c_status_t status, void* arg);

typedef struct {
    uint32_t frequency;
    uint32_t timeout_ms;

    ui2c_time_ms_t time_ms;
    void* time_arg;
} ui2c_config_t;

ui2c_status_t ui2c_init(ui2c_t* ui2c, const ui2c_config_t* config);

ui2c_status_t ui2c_xfer(ui2c_t* ui2c, uint16_t addr, const ui2c_msg_t* msgs, size_t num_msgs,
                        ui2c_callback_t callback, void* arg);

void ui2c_process(ui2c_t* ui2c);

#ifdef __cplusplus
}
#endif

#endif /* UI2C_UI2C_H */
