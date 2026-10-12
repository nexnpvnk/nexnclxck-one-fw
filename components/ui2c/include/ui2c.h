#ifndef UI2C_H
#define UI2C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI2C_VERSION_MAJOR 1
#define UI2C_VERSION_MINOR 0

typedef struct ui2c ui2c_t;

typedef enum {
    UI2C_STATUS_OK = 0,
    UI2C_STATUS_INVALID_ARG,
    UI2C_STATUS_NOT_INITIALIZED,
    UI2C_STATUS_BUSY,
    UI2C_STATUS_UNSUPPORTED,
    UI2C_STATUS_NACK,
    UI2C_STATUS_ARBITRATION_LOST,
    UI2C_STATUS_BUS_BUSY,
    UI2C_STATUS_BUS_ERROR,
    UI2C_STATUS_TIMEOUT,
    UI2C_STATUS_IO_ERROR,
    UI2C_STATUS_CANCELLED,
} ui2c_status_t;

enum {
    UI2C_MSG_WRITE = 0U,
    UI2C_MSG_READ = 1U << 0,
    UI2C_MSG_RESTART = 1U << 1,
};

/** Direction changes require RESTART on the new message. First START and final
 * STOP are automatic; no intermediate STOP. One zero-length WRITE is a probe. */
typedef struct {
    union {
        const uint8_t* tx;
        uint8_t* rx;
    } data;

    size_t len;
    uint16_t flags;
} ui2c_msg_t;

enum {
    UI2C_CAP_RESTART = 1U << 0,
    UI2C_CAP_CONTINUE = 1U << 1,
    UI2C_CAP_PROBE = 1U << 2,
};

typedef uint32_t (*ui2c_time_ms_t)(void* arg);
typedef void (*ui2c_callback_t)(ui2c_t* bus, ui2c_status_t status, void* arg);

/** Monotonic wrapping millisecond clock. timeout_ms is mandatory, 1..INT32_MAX.
 * Clock must run during a request. Service process at least once per timer wrap.
 * Board timing/frequency belongs to the backend, not this transport contract. */
typedef struct {
    ui2c_time_ms_t time_ms;
    void* time_arg;
    uint32_t timeout_ms;
} ui2c_config_t;

/** Submit a 7-bit controller transaction (unshifted address).
 * OK = accepted: exactly one callback follows from process. Otherwise rejected:
 * no callback or retained buffers. Callback is mandatory.
 * msgs, buffers and arg must live until callback. Do not modify descriptors/TX
 * data or access RX data during a request. No allocation, queue or auto retry.
 * All public calls are foreground-only, externally serialized (one event loop).
 * ISR/concurrent calls are forbidden. Backend IRQs are synchronized internally. */
ui2c_status_t ui2c_xfer(ui2c_t* bus, uint16_t addr, const ui2c_msg_t* msgs, size_t count, ui2c_callback_t callback,
                        void* arg);

/** Synchronously quiesce local hardware; callback remains deferred to process.
 * Already-completed result wins. No GPIO recovery: target may still hold SDA. */
ui2c_status_t ui2c_cancel(ui2c_t* bus);

/** Advance the backend, detect timeout and dispatch at most one callback.
 * IRQ, DMA and cooperative polling use the same application API.
 * Observed completion wins over timeout; this is not a hard completion deadline.
 * Idle before callback, so it
 * may submit the next request. Do not call process recursively.
 * Timeout is detected here, not by a hidden timer: service this regularly. */
void ui2c_process(ui2c_t* bus);

/** Includes completion waiting to be dispatched. NULL/uninitialized = false. */
bool ui2c_busy(ui2c_t* bus);

uint32_t ui2c_capabilities(const ui2c_t* bus);

#ifdef __cplusplus
}
#endif

#endif /* UI2C_H */
