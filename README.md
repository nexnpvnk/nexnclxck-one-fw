# nexnclxck-one-fw

[Русский](README_RU.md)

A personal pet project: a Nixie clock and an embedded MCU template.

Firmware for STM32F051x8 with six IN-12B tubes, four INS-1 indicators, RTC,
a high-voltage converter, WS2812 lighting, buttons, a buzzer and BME280.
Drivers access registers through CMSIS. Flash: 64 KiB. RAM: 8 KiB.

| Device | Initial settings |
| --- | --- |
| Digits and separators | 7 scan slots, 120 Hz full scan, 600 µs blanking, 100% brightness |
| Clock | HH:MM:SS, 24-hour format, leading zero; RTC clocked by LSE |
| Separator blinking | PC13 LOW enables the separator slot; about 500 ms per phase |
| DC/DC | 180 V, 24 kHz PWM, 1 kHz ADC/PI |
| Console | USART1, 1500000 baud, 8N1 |
| Lighting | 12 WS2812 LEDs, static warm orange (0xFF5008), brightness 64/255 (about 25%), speed 128/255 |
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
Short FN cycles `static` → `breath` → `rainbow` → `fade` → `flicker` → `comet`
→ `static` on release outside the editor. Hold FN for 1 second to enter at
setting 1. Inside, short FN selects the next setting: 1 brightness, 2 speed,
3 primary color, 4 secondary color, then 1 again.
The leftmost tube shows the setting number; the value is aligned to the right.
LEFT / RIGHT changes brightness/speed by one point in 0..255, or cycles a
16-color palette for primary/secondary. Color settings preview the selected
color as static at current brightness; leaving them resumes the selected effect.
Holding LEFT / RIGHT starts repeat after 400 ms and speeds it up after 1.2 and
2.5 seconds. The initial press beeps once; holding and repeat are silent.
OK or 10 seconds of inactivity return to time. Changes apply immediately
and remain in RAM until reset. Releasing the initial FN hold keeps adjustment open.

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
[upper-board profile documentation](apps/app_display/README.md). The hardware
profile in `apps/app_display/src/display_profile.c` sets the actual element count,
wiring, tube capabilities and physical digit/point positions on the upper board.
Unused physical elements are omitted. Scan slots remain constant across screens.
The physical WS2812 count is set only by `led_count` in that same upper-board
profile. `app_main` reads it through `app_display_profile()` before hardware
initialization. The driver has storage for up to 16 LEDs and sends the requested
count; set the profile to 8 or 14 for a different upper board.

## Structure

| Directory | Responsibility |
| --- | --- |
| `platform/` | MCU startup, system clocks, interrupt vectors and libc hooks |
| `bsp/` | Board GPIO, peripheral drivers and hardware fault handling |
| `apps/` | Clock UI, display control, HV, sensor, lighting, buttons, sound and CLI |
| `components/` | Libraries and hardware-independent components |
| `ports/ui2c/stm32f0/` | Register-based ui2c backend for STM32F0 |
| `cmake/` | Toolchain, firmware composition and OpenOCD targets |
| `debug/` | OpenOCD configuration and SVD |

`main()` configures system clocks, calls `bsp_board_init()`, then `app_main()`.
The main loop services applications, ui2c and the console. Interrupt vectors
dispatch to BSP handlers. Each hardware resource has one owner. Source files
and CMake configuration are maintained manually;
[the .ioc file](nexnclxck-one-fw.ioc) is a pin-assignment reference.

`app_input` recognizes button presses, releases and holds. `app_ui` chooses their
actions, sound feedback and screen content; button-based clock settings will live
here; browsing and lighting brightness adjustment are implemented.
`app_display` owns the Nixie profile,
brightness and plan publication through `nixie` / `bsp_nixie`. The hardware profile
is immutable; live brightness and timing are stored separately. `app_main` connects
the modules and passes permission to render without blocking UI input. WS2812
lighting is handled separately by `app_light` / `ledfx` / `bsp_ws2812`.
CLI is an additional service interface.

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

## C coding style

The project uses the [personal C style fork](https://github.com/nexnpvnk/c-code-style)
as a pinned submodule in `tools/c-code-style`. Its
[coding rules](tools/c-code-style/CODING_RULES.md), formatter configuration, and
reference templates remain in that repository.

Use LLVM 23.x for both `clang-format` and `clangd`, preferably the same version.
The root `.clang-format` redirects to the submodule configuration. Initialize
submodules as shown above before formatting. LLVM is a development tool here;
the firmware still builds with Arm GNU Toolchain.

Codex uses the [project skill adapter](.agents/skills/c-code-style/SKILL.md)
alongside [AGENTS.md](AGENTS.md). The adapter links to the canonical skill and
provides native formatter commands that work in PowerShell. Apply the style
only to project-authored files within the task's scope. Imported and adapted
vendor, library, generated, and external-template files retain their original
formatting; see [AGENTS.md](AGENTS.md) for the project's exclusions.

To review and adopt the latest style revision explicitly:

```powershell
git submodule update --remote -- tools/c-code-style
git diff --submodule=log -- tools/c-code-style
git add tools/c-code-style
```

Commit the updated submodule pointer in the firmware repository after reviewing
the changes. Ordinary `git submodule update --init --recursive` restores the
recorded revision.

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
| Brightness 0..100%, refresh 1..400 Hz, blanking in µs | `display set -b 100 -r 120 -t 600` |
| Lighting parameters | `light` |
| Lighting effect | `light set --effect fade` |
| Primary and secondary RGB colors | `light set --primary 0xFF5008 --secondary 0x0020FF` |
| Lighting speed and brightness, 0..255 | `light set --speed 128 --brightness 64` |
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
Brightness 0 turns lighting off, 255 gives full brightness. UI, CLI and the
renderer all use the full 8-bit range. RGB colors remain `0xRRGGBB`. Options can be
changed independently; omitted values are preserved. Short forms are `-e`
for effect, `-s` for speed and `-b` for brightness. Changes are rendered from
the main loop and sent asynchronously via DMA. Settings are held in RAM
and return to startup defaults after reset.
`light reset` restores `static`, colors `0xFF5008` / `0x0020FF`, speed 128/255
and brightness 64/255 (about 25%), and restarts the effect phase while preserving the LED count.
Animated effects render every 33 ms; static lighting renders when needed.
Custom RGB colors remain available through the CLI. The color editor shows 0
when a color is outside its palette, preserving it until LEFT / RIGHT selects
an entry. The palette is listed in the [UI documentation](apps/app_display/README.md).

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
