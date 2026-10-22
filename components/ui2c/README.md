# ui2c

**Portable, non-blocking I²C controller library for bare-metal embedded systems.**

[Русская версия](README_RU.md)

> **Status:** pre-release. The portable core and its v1 API are implemented. An STM32F051 backend has been developed in the original firmware project, but complete hardware validation is still pending. Support for other MCUs must not be assumed.

## Why ui2c?

Microcontroller vendors expose different peripheral interfaces, transfer APIs, interrupt models and DMA controllers. Device drivers should not need to know those details. `ui2c` provides a small, consistent transaction API while leaving hardware-specific execution to a dedicated backend.

The library is designed for projects that need portable I²C clients, explicit error handling and predictable non-blocking behavior without depending on an RTOS or a vendor HAL.

## Design principles

- **Hardware-independent core.** No CMSIS, HAL, RTOS or board headers in the core.
- **One transaction API.** Applications submit message sequences, not hardware register operations.
- **Non-blocking operation.** Submission never waits for a transfer to finish; completion is reported through a callback dispatched by `ui2c_process()`.
- **Backend-owned hardware.** IRQs, DMA, polling, START/RESTART/STOP sequencing and MCU errata are handled by the port.
- **Explicit capabilities.** Unsupported message combinations are rejected rather than silently altered.
- **No dynamic allocation or hidden scheduler.** The core owns no heap, transaction queue or automatic retries.
- **Correct error boundaries.** Arbitration loss, NACK, bus busy, cancellation and timeout are reported to the client; retry policy remains outside the library.
- **Board neutrality.** Pinmux, clocks, bus timing and peripheral/DMA resource assignments belong to the BSP.

## Architecture

```text
Application / device driver    transactions, retry/backoff policy
             |
          ui2c.h              portable public API
             |
          ui2c.c              validation, lifecycle, timeout, callback
             |
        ui2c_port.h           backend contract
             |
       MCU-specific port      hardware state machine, IRQ/DMA/polling
             |
             BSP              pins, clocks, routing, initialization
```

The **core** and the **backend contract** form the `ui2c` repository. Hardware ports may live in a firmware project or in independently maintained port repositories; they are **not** required to be embedded into the `ui2c` Git submodule. A board-specific BSP is never part of the portable core.

## Basic usage

```c
#include "ui2c.h"

static uint8_t chip_id_register = 0xD0;
static uint8_t chip_id;

/* Descriptors and buffers must remain valid until the callback. */
static const ui2c_msg_t messages[] = {
    { .data.tx = &chip_id_register, .len = 1, .flags = UI2C_MSG_WRITE },
    { .data.rx = &chip_id, .len = 1, .flags = UI2C_MSG_READ | UI2C_MSG_RESTART },
};

static void on_complete(ui2c_t* bus, ui2c_status_t status, void* arg) {
    (void)bus;
    (void)arg;
    if (status == UI2C_STATUS_OK) {
        /* chip_id is now available. */
    }
}

ui2c_status_t read_chip_id(ui2c_t* bus) {
    const ui2c_addr_t address = {
        .value = 0x76,       /* Unshifted 7-bit address. */
        .mode = UI2C_ADDR_7BIT,
    };
    return ui2c_xfer(bus, address, messages, 2, on_complete, NULL);
}

/* Call ui2c_process(bus) regularly from the foreground event loop. */
```

A successful `ui2c_xfer()` return means the request was **accepted**, not that the bus operation has finished. The completion callback is delivered from `ui2c_process()`, outside interrupt context.

## Execution contract

- The public API is called from one externally serialized foreground context; concurrent/ISR calls are not supported by the core contract.
- One request at a time is allowed per bus, including a completion waiting for callback dispatch.
- Message descriptors, TX/RX buffers and the callback context must remain valid until the callback.
- The backend owns hardware activity and must stop accessing request memory before completing or aborting a transaction.
- The monotonic millisecond clock is supplied by the integration; `ui2c_process()` must run regularly for timeout detection.
- `ui2c_cancel()` only cancels local controller activity. It cannot guarantee the external I²C lines are physically released.
- The core does not impose automatic bus recovery, arbitration retry or a multi-controller scheduling policy.

## Integration

The portable core can be linked using CMake:

```cmake
add_subdirectory(components/ui2c)
target_link_libraries(your_target PRIVATE ui2c::core)
```

A separately maintained MCU port links against the core and supplies the peripheral implementation, initialization API and IRQ integration. The BSP configures the hardware and creates the port instance.

## Repository mirrors

**GitHub is the authoritative upstream; GitFlic is an official read-only mirror.** Development changes, issues and releases are managed upstream. The GitFlic repository address will be added once the mirror is created and verified. Release tags are intended to be identical on both hosts.

## Scope and roadmap

The current project focuses on I²C controller mode. Target/slave mode, automatic retries, queueing, SMBus features and universal bus recovery are not part of the current core contract. Additional MCU support will be documented only after implementation and verification.

## License

Licensed under the **Apache License, Version 2.0**. See [LICENSE](LICENSE).
