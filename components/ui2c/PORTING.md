# Porting ui2c to a new platform

[Русская версия](PORTING_RU.md) | [Project README](README.md)

## Overview

`ui2c` separates the portable I²C transaction contract from the hardware-specific controller implementation.

The application describes an I²C transaction. The platform backend decides how to execute it using peripheral registers, interrupts, DMA, or cooperative polling.

A transaction, an API message, an I²C address phase, and a DMA transfer are distinct concepts. A backend must preserve the requested bus sequence regardless of how hardware operations are divided internally.

The portable core and backend interface are provided by:

- `include/ui2c.h` — public application API.
- `include/ui2c_port.h` — backend interface and shared controller state.
- `src/ui2c.c` — validation, transaction lifecycle, timeout management, and callback dispatch.

Hardware-specific backends may reside in firmware projects or independently maintained repositories. They are not required to live inside the `ui2c` repository.

## Responsibilities

| Layer | Responsibilities |
|---|---|
| Application / device driver | Transactions, device protocol, retries, backoff, scheduling |
| ui2c core | Validation, request lifecycle, timeout detection, callback delivery |
| MCU backend | Hardware transactions, START/RESTART/STOP, IRQ, DMA, arbitration and peripheral errors |
| BSP | Clocks, pinmux, timing configuration, resource allocation, IRQ routing and controller instances |

The backend must not implement application-specific retry policies or depend on particular I²C devices.

The core must not depend on MCU registers, vendor HALs, CMSIS, an RTOS, or board-specific configuration.

## Backend interface

A backend includes `ui2c_port.h` and provides persistent storage containing a `ui2c_t` instance and any hardware-specific state.

Four operations are mandatory:

| Operation | Contract |
|---|---|
| `lock()` | Serialize foreground access with all relevant backend IRQs |
| `unlock()` | Restore the previous interrupt/synchronization state |
| `start()` | Start a transaction without blocking |
| `abort()` | Synchronously stop local access to transaction memory |

Two operations are optional:

| Operation | Contract |
|---|---|
| `validate()` | Validate MCU-specific transaction restrictions without side effects |
| `process()` | Perform bounded cooperative polling or deferred backend work |

The backend registers these operations through `ui2c_port_init()`.

The backend instance, operations table, and hardware resources must remain valid throughout the controller lifetime.

The controller context must be zero-initialized before its first initialization.

Initialization must not depend on the external I²C bus being idle.

### Synchronization

`lock()` and `unlock()` protect shared state from concurrent interrupt access. They must:

- Synchronize all backend IRQ sources, including DMA interrupts.
- Preserve and restore the previous interrupt state.
- Provide appropriate compiler and memory ordering.
- Never wait for an interrupt, mutex, or external event.

A pure polling backend without concurrent interrupt access may use no-op synchronization functions.

Public `ui2c` API calls are restricted to a single externally serialized foreground context. They must not be invoked concurrently or from an ISR.

## Transaction semantics

The backend receives the complete transaction description, including the target address and message array.

Addresses are unshifted. The core supports a common 7-bit address representation and an optional 10-bit mode.

The first START and final STOP are implicit.

For consecutive messages:

- `UI2C_MSG_RESTART` requests a repeated START before the following message.
- Messages without RESTART continue without another START or address phase.
- Changing direction requires RESTART.
- An unsupported message combination must be rejected before the transaction starts.

A single zero-length write is an address-only probe when `UI2C_CAP_PROBE` is supported.

Use `ui2c_port_boundary()` to determine whether the next boundary requires CONTINUE, RESTART, or STOP.

Hardware chunk boundaries and DMA buffer boundaries must not introduce additional START or STOP conditions.

## Capabilities

The backend declares only capabilities it actually implements:

| Capability | Meaning |
|---|---|
| `UI2C_CAP_RESTART` | Repeated START between messages |
| `UI2C_CAP_CONTINUE` | Consecutive messages without a new address phase |
| `UI2C_CAP_PROBE` | Address-only zero-length write |
| `UI2C_CAP_ADDR_10BIT` | 10-bit I²C addressing |

Capabilities describe bus protocol support, not whether a backend uses IRQ or DMA.

If a controller supports a capability only for certain combinations, implement the optional `validate()` operation to reject unsupported combinations before hardware activity.

A backend must never silently replace a repeated START with STOP followed by START.

## Initialization and request lifecycle

A typical backend initialization sequence is:

1. The BSP configures clocks, pins, peripheral timing, DMA routing, and interrupt resources.
2. The backend validates the supplied hardware configuration.
3. The backend calls `ui2c_port_init()` with its operations and supported capabilities.
4. The backend prepares its local hardware state for subsequent transactions.

The backend owns the peripheral during transfer operations. Other software must not simultaneously control the same I²C hardware or DMA channels.

The core uses the following states:

`IDLE → ACTIVE → DONE → IDLE`

`ui2c_xfer()` returning `UI2C_STATUS_OK` means the request was accepted, not completed.

For every accepted request, exactly one completion callback is subsequently dispatched by `ui2c_process()`.

A rejected request retains no message buffers and produces no callback.

### start()

`start()` executes under the backend lock and must return promptly.

It must not wait for the external bus to become idle.

If hardware is currently owned by another controller, return `UI2C_STATUS_BUS_BUSY` where appropriate.

An unsuccessful start must leave the local hardware quiescent, with no active access to the request buffers.

A backend may complete a transaction synchronously during `start()`, but it must then return `UI2C_STATUS_OK`. The application callback is still deferred to `ui2c_process()`.

### Completion

Call `ui2c_port_complete()` only after the transaction result is final and no hardware access to request memory remains.

In particular, DMA completion does not necessarily mean I²C transaction completion. Ensure all required peripheral events, DMA writes, and cache operations have completed.

The backend must prevent stale IRQ or DMA events from completing a subsequent request.

The core dispatches the application callback outside the backend lock and outside interrupt context.

The controller is already IDLE when the callback executes, so the application may submit another transaction.

### Cancellation and timeout

`abort()` executes under the backend lock and must synchronously stop all local IRQ/DMA access to transaction memory.

It must not dispatch the application callback or directly change the core lifecycle.

The core completes an aborted active request with `UI2C_STATUS_CANCELLED` or `UI2C_STATUS_TIMEOUT` as appropriate.

Already observed completion takes precedence over cancellation or timeout.

Timeout detection is cooperative: `ui2c_process()` must be called regularly. The supplied millisecond clock must be monotonic, with `uint32_t` wraparound supported and `timeout_ms` restricted to `1..INT32_MAX`. An active request must not be left unprocessed for an entire clock wrap.

Timeout is not a strict hardware completion deadline.

Cancellation stops local controller activity but does not guarantee physical release of SCL or SDA by external devices.

## IRQ, DMA, and polling

A backend may use:

- Interrupt-driven transfers.
- DMA-assisted transfers.
- Cooperative polling.
- A combination of these mechanisms.

These implementation choices must not change the public API.

The optional backend `process()` operation runs in foreground, before core timeout evaluation. It must perform bounded, non-blocking work.

It may be invoked while the controller is IDLE, ACTIVE, or DONE. Request buffers must only be accessed while the request is ACTIVE.

Backend callbacks must never recursively invoke the public API or application callbacks.

### DMA and cache coherency

DMA ownership and memory synchronization belong to the backend.

For cache-enabled MCUs, carefully account for cache-line alignment, TX clean operations, RX invalidation, and memory regions inaccessible to DMA.

Invalidating a partially shared cache line may destroy unrelated dirty data. Use DMA-safe memory, an aligned bounce buffer, or an interrupt-driven fallback when necessary.

`abort()` must also guarantee that pending DMA activity cannot access a buffer after the request has been cancelled.

## Multi-controller operation

The backend must correctly handle arbitration loss, external bus ownership, and controller-specific bus state.

On arbitration loss:

- Stop local transaction processing.
- Release local resources without disturbing the winning controller.
- Report `UI2C_STATUS_ARBITRATION_LOST`.
- Do not force STOP or perform GPIO bus recovery.

Automatic retry is intentionally outside the core and backend contract.

The application decides when retry is appropriate, including whether partially executed writes can be safely repeated.

`UI2C_STATUS_BUSY` represents local controller/resource occupancy; `UI2C_STATUS_BUS_BUSY` represents physical bus occupancy.

## Platform research

The following notes describe investigated implementation requirements, not a list of verified supported backends.

| MCU | Relevant hardware characteristics | Suggested backend approach |
|---|---|---|
| STM32F051 | NBYTES, RELOAD, TC/TCR, AUTOEND; ES0202 errata | Register-based state machine with IRQ/DMA and RX RELOAD workarounds |
| NXP MK64 | I²C DMA requests interact with interrupt-driven protocol handling; last RX byte requires explicit ACK/NACK control | IRQ-controlled state machine with optional eDMA for intermediate payload bytes |
| ATSAME70Q | TWIHS, XDMAC, internal address and alternative command modes; cache coherency on Cortex-M7 | Hybrid IRQ/DMA backend with transaction capability validation |
| K1921VG015 | I²C status-machine events and interrupt routing through RISC-V PLIC | IRQ-driven state machine; DMA only if confirmed for the exact device revision |

The existing STM32F051 implementation is maintained in the firmware project's `ports/ui2c/stm32f0/` directory, outside the portable core. Full hardware validation remains pending.

References:

- [STM32F051 errata ES0202](https://www.st.com/resource/en/errata_sheet/es0202-stm32f051x4x6x8-device-errata-stmicroelectronics.pdf)
- [NXP K64 Reference Manual](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/kinetis/25442/2/K64P144M120SF5RM.pdf)
- [Microchip SAM E70/S70/V70/V71 data sheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/SAM-E70-S70-V70-V71-Family-Data-Sheet-DS60001527.pdf)
- [NIIET K1921VG015 reference manual](https://niiet.ru/wp-content/uploads/2024/10/%D0%A0%D0%9F-%D0%9A1921%D0%92%D0%93015-v3.pdf)

Platform-specific behavior must be rechecked against the current documentation and the exact device revision before implementing a backend.

## Backend verification

Before declaring a new backend supported, verify:

1. Address ACK/NACK, zero-length probe, normal read and write.
2. Repeated START and CONTINUE semantics, without unintended STOP/START.
3. Message sizes around hardware and DMA limits.
4. NACK, arbitration loss, bus busy, timeout, and cancellation.
5. No stale IRQ/DMA access after completion or cancellation.
6. Repeated transfers after errors and recovery of local controller state.
7. Bus behavior under competing controllers, where applicable.
8. Device-specific errata, timing, cache, DMA, and IRQ interactions.

Hardware validation should use a logic analyzer where possible.

A successful compilation alone does not establish protocol correctness or multi-controller reliability.
