# nexnclxck-one-fw

[Русский](README_RU.md)

A personal pet project: a Nixie clock and an embedded MCU template.

Firmware for STM32F051x8 with six IN-12B tubes, four INS-1 indicators, RTC,
a high-voltage converter, WS2812 lighting, buttons, a buzzer and BME280.
Drivers access registers through CMSIS. Flash: 64 KiB. RAM: 8 KiB.

| Device | Initial settings |
| --- | --- |
| Digits and separators | 7 scan slots, 120 Hz full scan, 500 µs blanking, 100% brightness |
| Clock | HH:MM:SS, 24-hour format, leading zero; RTC clocked by LSE |
| Separator blinking | PC13 LOW enables the separator slot; about 500 ms per phase |
| DC/DC | 180 V, 24 kHz PWM, 1 kHz ADC/PI |
| Console | USART1, 1500000 baud, 8N1 |
| Lighting | 12 WS2812 LEDs, rainbow, brightness 32/255, 33 ms effect interval |
| BME280 | Forced mode, 500 ms measurement interval |

The display is blank until time is valid and HV is ready. Device settings live
in RAM; the RTC calendar and validity marker live in the backup domain.

## Structure

| Directory | Responsibility |
| --- | --- |
| `platform/` | MCU startup, system clocks, interrupt vectors and libc hooks |
| `bsp/` | Board GPIO, peripheral drivers and hardware fault handling |
| `apps/` | Clock, display, HV, sensor, lighting, buttons, sound and CLI |
| `components/` | Libraries and hardware-independent components |
| `ports/ui2c/stm32f0/` | Register-based ui2c backend for STM32F0 |
| `cmake/` | Toolchain, firmware composition and OpenOCD targets |
| `debug/` | OpenOCD configuration and SVD |

`main()` configures system clocks, calls `bsp_board_init()`, then `app_main()`.
The main loop services applications, ui2c and the console. Interrupt vectors
dispatch to BSP handlers. Each hardware resource has one owner. Source files
and CMake configuration are maintained manually;
[the .ioc file](nexnclxck-one-fw.ioc) is a pin-assignment reference.

- [BSP hardware contracts](bsp/README.md) (Russian)
- [Display and upper-board profile](apps/app_display/README.md) (Russian)
- [BME280 measurements and shared bus](apps/app_bme280/README.md) (Russian)

## Build

Requires CMake 3.22+, Ninja and Arm GNU Toolchain (`arm-none-eabi-*`) on PATH.
OpenOCD and ST-Link are required for programming and debugging targets.

```powershell
git submodule update --init --recursive
cmake --preset Release
cmake --build --preset Release
```

ELF: `build/Release/nexnclxck-one-fw.elf`. The `Debug` preset uses `-Og -g3`;
`Release` uses `-Os -g0` and LTO when supported by the toolchain. Linker script:
`STM32F051xx_FLASH.ld`. OpenOCD targets: `flash` (program/verify/reset),
`reset` and `erase` (the entire Flash).

## Console

```text
wsh --exec
clock set -d 2026-11-14 -t 12:34:56
```

| Task | Command |
| --- | --- |
| RTC status and calendar | `clock` |
| Set date and time | `clock set -d 2026-11-14 -t 12:34:56` |
| Invalidate the calendar (admin) | `clock invalidate` |
| Latest BME280 result / monitor | `bme280` / `bme280 -w` |
| HV status / monitor | `hv` / `hv -w` |
| HV target, 100..250 V | `hv set -v 180` |
| HV PWM, 20000..50000 Hz | `hv set -f 24000` |
| Request HV operation / disable HV | `hv on` / `hv off` |
| Buzzer status | `buzzer` |
| Frequency 100..10000 Hz, volume 0..100% | `buzzer set -f 2500 -v 20` |
| Manual buzzer enable / disable | `buzzer on` / `buzzer off` |
| Display status | `display` |
| Brightness 0..100%, refresh 1..400 Hz, blanking in µs | `display set -b 100 -r 120 -t 500` |

`set` options can be combined. Date and time must be supplied together;
supported years are 2000..2099. Numeric options accept unsigned decimal values
without suffixes. All arguments are validated before changes are applied.
Unknown and repeated options are rejected. `-h` / `--help` shows help without
performing an action.

One monitor can run at a time. It prints an initial snapshot and updates once
per second without replaying missed intervals. Ctrl+C stops output. Devices
are disabled with `hv off` and `buzzer off`.

HV watch shows filtered voltage/target, Duty, PWM frequency, measured ADC rate
and application/BSP faults. The initial ADC rate is `n/a`. `hv on` confirms
the request; status reports the actual state. `hv off` also cancels automatic
restart attempts.

BME280 `Age` is the elapsed time since the latest completed result. Shared
I2C bus contention defers acquisition while the cached result gets older.
