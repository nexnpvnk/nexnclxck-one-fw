## Sensor lifecycle

The application initializes the BME280 on its first polling attempt.
Bosch SensorAPI performs chip identification, soft reset and calibration
loading during initialization. Oversampling and filter settings are
configured once.

The Bosch device and transport contexts persist between measurements.
Normal acquisition runs every 500 ms using forced mode, waits for the
Bosch-calculated conversion time and reads compensated sensor data.

Initialization is not repeated on successful samples.

After three consecutive non-contention measurement failures, the
cached sensor context is invalidated. Initialization is retried at
1000 ms intervals until successful.

UI2C_BUSY, UI2C_BUS_BUSY and UI2C_ARBITRATION_LOST are treated as
temporary bus contention and do not trigger sensor reinitialization.

The sensor is probed at 0x76 first, then at 0x77 only for NACK or
an incorrect chip ID. Calibration is reloaded after recovery.

The existing acquisition path remains synchronous and foreground-only.
A sensor conversion or I2C timeout can therefore temporarily delay
other application processing.
