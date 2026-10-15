## Shared-bus policy

BME280 uses the same ui2c controller as other application clients.

Before starting an acquisition, the application checks whether a local
transfer is already pending. If the controller is busy, acquisition is
deferred without entering Bosch SensorAPI.

UI2C_STATUS_BUSY, UI2C_STATUS_BUS_BUSY and
UI2C_STATUS_ARBITRATION_LOST are treated as temporary bus contention.

Bus contention:
- Does not invalidate the Bosch device context.
- Does not trigger a sensor reset.
- Does not increment or clear the sensor failure counter.
- Does not overwrite the cached measurement.
- Schedules another attempt after 50 ms.

UI2C_STATUS_NOT_INITIALIZED is treated as temporary infrastructure
unavailability and retried after 1000 ms.

Successful measurements are scheduled every 500 ms. Actual
non-contention measurement failures use the existing three-failure
recovery policy.

When acquisition is deferred, the previous cached sample remains
available. Consumers must use sampled_at_ms to determine its age.
