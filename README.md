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
| BME280 | Forced mode, 100 ms measurement interval |

The display shows the RTC counters once RTC and HV are ready, even when the
time has not been set. Device settings live in RAM; the RTC calendar and
time-set marker live in the backup domain.

Presentation supports a profile with four digit tubes and two central points,
or six tubes and four points. Six-digit time/date formats are `HH:MM:SS` and
`DD.MM.YY`; four-digit formats are `HH:MM` and `DD.MM`. The cycle shows time
for 40 seconds, then date, temperature, humidity and pressure for 5 seconds each.
Temperature is in °C and humidity in %, both rounded to tenths; pressure is
rounded to integer mmHg. Unavailable data screens show time. Sensor readings are
unavailable until the first successful measurement and after an acquisition error;
the next successful measurement makes them available again.

RIGHT / LEFT steps through available screens and holds the selection for
10 seconds after the last press, then returns to time and the automatic cycle.
The configured tube's internal decimal point takes priority for fractional
values (IN-12B); tubes without one (IN-12A) use the lower external point.
The IN-12B point is to the left of its digit, so `23.4` lights it on the tube
showing `4`.
The upper point of the last separator marks a negative temperature. On four
tubes without an internal point, values that do not fit with a decimal are
rounded to integers, including `100.0` % → `100` %.

The current upper board uses digit anodes A6, A5, A4, A3, A1, A0 from left to
right and a shared A2 for the four points:

| Point | Cathode |
| --- | --- |
| Upper left | C7 |
| Upper right | C3 |
| Lower left | C10 |
| Lower right | C13 |

The complete digit cathode map and the four-digit layout example are in the
[upper-board profile documentation](apps/app_display/README.md). The profile
sets the actual element count, wiring, logical positions and tube capabilities;
unused physical elements are omitted. Scan slots remain constant across screens.

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
| Clear the time-set marker (admin) | `clock invalidate` |
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
| Lighting parameters | `light` |
| Lighting effect | `light set --effect fade` |
| Primary and secondary RGB colors | `light set --primary 0xFF5008 --secondary 0x0020FF` |
| Lighting speed and brightness, 0..255 | `light set --speed 128 --brightness 32` |
| Turn lighting off | `light set --brightness 0` |
| Restore startup lighting parameters | `light reset` |

`set` options can be combined. Date and time must be supplied together;
supported years are 2000..2099. Numeric options accept unsigned decimal values
without suffixes; lighting colors use `0xRRGGBB`. All arguments are validated
before changes are applied. Unknown and repeated options are rejected.
`-h` / `--help` shows help without performing an action.

`light` shows the requested lighting parameters and LED count. Effects are
`static`, `breath`, `rainbow`, `fade`, `flicker` and `comet`. `rainbow` ignores
both colors; `fade` uses both; the other effects use the primary color.
Speed 0 is the slowest setting, 255 the fastest; `static` ignores speed.
Brightness 0 turns lighting off, 255 gives full brightness. Options can be
changed independently; omitted values are preserved. Short forms are `-e`
for effect, `-s` for speed and `-b` for brightness. Changes are rendered from
the main loop and sent asynchronously via DMA. Settings are held in RAM
and return to startup defaults after reset.
`light reset` restores `rainbow`, colors `0xFF5008` / `0x0020FF`, speed 128
and brightness 32, and restarts the effect phase while preserving the LED count.

`clock` shows the RTC date and time whenever they can be read, with a `not set`
warning if the time-set marker is absent. Log timestamps use RTC time when it
can be read and uptime otherwise. `clock invalidate` clears the marker while
the RTC counters and display continue running.

One monitor can run at a time. It prints an initial snapshot and updates once
per second without replaying missed intervals. Ctrl+C stops output. Devices
are disabled with `hv off` and `buzzer off`.

HV watch shows filtered voltage/target, Duty, PWM frequency, measured ADC rate
and application/BSP faults. The initial ADC rate is `n/a`. `hv on` confirms
the request; status reports the actual state. `hv off` also cancels automatic
restart attempts.

BME280 shows temperature, pressure in integer mmHg and humidity, or `readings unavailable`.
Readings have no age limit and rely on regular acquisition. A read deferred before
accessing the busy I2C bus preserves availability; a failed acquisition clears it.
