#ifndef UI2C_PORT_H
#define UI2C_PORT_H

#include "ui2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Backend interface, not an application header. No MCU/RTOS dependencies.
 * lock/unlock synchronize foreground with ALL backend IRQs, preserve interrupt
 * state and provide compiler/memory ordering. They must not wait for a mutex.
 * start/abort are bounded and called locked. Failed start MUST be quiescent.
 * abort MUST synchronously stop every IRQ/DMA access to request memory.
 * start may complete synchronously, but must then return OK; callback is deferred.
 * Optional validate checks the supplied address/messages without side effects,
 * unlocked, before publication in bus (which may still hold an older request).
 * Optional process runs unlocked in foreground before timeout evaluation. It must
 * do bounded, nonblocking work and synchronize with IRQs itself. It may be called
 * in any state; only ACTIVE permits request memory access. Never call application
 * callbacks or recursively enter public API from port operations. */
typedef uintptr_t ui2c_lock_key_t;

typedef struct {
    ui2c_lock_key_t (*lock)(ui2c_t* bus);
    void (*unlock)(ui2c_t* bus, ui2c_lock_key_t key);
    ui2c_status_t (*validate)(ui2c_t* bus, uint16_t addr, const ui2c_msg_t* msgs, size_t count);
    ui2c_status_t (*start)(ui2c_t* bus);
    void (*abort)(ui2c_t* bus);
    void (*process)(ui2c_t* bus);
} ui2c_port_ops_t;

typedef enum { UI2C_IDLE, UI2C_ACTIVE, UI2C_DONE } ui2c_state_t;

typedef enum { UI2C_BOUNDARY_CONTINUE, UI2C_BOUNDARY_RESTART, UI2C_BOUNDARY_STOP } ui2c_boundary_t;

struct ui2c {
    const ui2c_port_ops_t* ops;
    void* port;
    ui2c_config_t config;
    uint32_t capabilities;
    const ui2c_msg_t* msgs;
    size_t num_msgs;
    uint16_t addr;
    ui2c_callback_t callback;
    void* callback_arg;
    uint32_t started_at;
    volatile ui2c_state_t state;
    volatile ui2c_status_t result;
};

/** Zero-initialized persistent storage; initialize once before backend IRQ access.
 * ops and port must remain valid for the bus lifetime; config is copied.
 * Local initialization must not require an idle external bus. */
ui2c_status_t ui2c_port_init(ui2c_t* bus, const ui2c_port_ops_t* ops, void* port, const ui2c_config_t* config,
                             uint32_t capabilities);
/** Call serialized, ONLY AFTER hardware is quiescent and RX/cache work finished.
 * Duplicate completion is ignored while not ACTIVE. Backend must suppress stale
 * events before a new request; this function cannot identify an old completion. */
void ui2c_port_complete(ui2c_t* bus, ui2c_status_t status);
ui2c_boundary_t ui2c_port_boundary(const ui2c_t* bus, size_t msg_index);

#ifdef __cplusplus
}
#endif

#endif /* UI2C_PORT_H */
