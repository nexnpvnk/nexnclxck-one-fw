# BME280: Bosch SensorAPI over ui2c

The unmodified Bosch library owns initialization, settings, calibration and
compensation. The private src/bme280_port.c adapter supplies synchronous read/write/delay hooks.
There is no separate sensor state machine. apps/app_bme280 owns periodic
acquisition and the latest result; the console is only a consumer.

## Console

```text
bme280
bme280 --watch
bme280 --stop
bme280 --help
```

The application polls immediately on the first main-loop iteration, then every
500 ms between starts, without catch-up reads after a delayed iteration. This is
cooperative blocking scheduling, not a hard real-time sampling guarantee.
Acquisition continues without a logged-in user and with console output stopped.
The latest attempt contains values, completion timestamp, address and error
status. Failed attempts invalidate values instead of presenting old data as fresh.

The sensor module tries 0x76, then 0x77 only for NACK or a wrong chip ID.
Both addresses are 7-bit, unshifted. BMP280 is not accepted as BME280.
The console no longer accepts --addr: it does not configure the sensor.

Each sample calls bme280_init, bme280_set_sensor_settings,
bme280_set_sensor_mode(FORCED), waits the Bosch-calculated conversion time,
and calls bme280_get_sensor_data. Reinitializing each sample avoids cached
configuration/calibration after a reset or sensor replacement. Oversampling
is x1 on all channels, with the IIR filter disabled.

The bme280 command displays the latest cached result without I2C activity.
Watch displays that cache once per second. Ctrl+C and --stop stop only console
output; acquisition continues every 500 ms. Logout or entering another
interactive command also stops watch. A synchronous measurement already running
finishes before the console handles input. A subsequent successful poll replaces
an error automatically, whether or not watch is enabled.

```text
BME280 0x76 | T: 25.08 C | P: 1006.53 hPa | RH: 50.72 %
```

Example values only. Pressure is absolute; humidity is relative. Bosch integer
64-bit compensation returns temperature in 0.01 C, pressure in 0.01 Pa and
humidity in 1/1024 %RH. The sensor module caches humidity in 0.01 %RH;
the console formats the cached values for display.

## I2C connection points

- apps/app_bme280/src/app_bme280.c: read_sample selects bsp_i2c_bus(),
  the address and bsp_time_ms, then binds a struct bme280_dev.
- apps/app_bme280/src/bme280_port.c: bme280_port_bind installs dev.read, dev.write,
  dev.delay_us and dev.intf_ptr. Bosch invokes these callbacks internally.
- read_regs sends the register address, then reads using a repeated START.
- write_regs sends the register address and Bosch payload in one continuous
  write, with no intermediate STOP or repeated START.
- transfer submits ui2c_xfer and calls ui2c_process until its completion callback.
  The backend may use IRQ, DMA or cooperative polling; the adapter is identical.
- bsp/src/bsp_i2c.c binds the bus to I2C1 and configures a 100 ms transfer timeout.
- Core/Src/i2c.c configures PB6 as SCL and PB7 as SDA, AF1 open drain, without
  internal pull-ups. The physical bus needs external pull-ups.

The adapter is foreground-only: interrupts and the monotonic timeout clock must
run. Never invoke Bosch operations from a ui2c completion callback or ISR.
Accepted transfers finish or time out through ui2c_process before stack buffers
are released. BUSY is returned immediately without cancelling another client.
There are no retries inside the adapter and no GPIO bus recovery. The timeout
applies to each transfer, not to a complete multi-transfer Bosch operation.
Millisecond timing rounds Bosch delays upward, including a tick-phase margin.

Compared with the previous custom driver, validation now follows Bosch SensorAPI:
there is no extra raw-data/calibration plausibility filter or overall 500 ms
measurement deadline. Obsolete tests for that removed async API were removed.
