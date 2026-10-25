#include "app_bme280.h"
#include "bme280_port.h"
#include "bsp_i2c.h"
#include "bsp_time.h"

#define BME280_SAMPLE_INTERVAL_MS    500U
#define BME280_RETRY_INTERVAL_MS     1000U
#define BME280_BUS_RETRY_INTERVAL_MS 50U
#define BME280_FAILURE_LIMIT         3U

static const struct bme280_settings settings = {
    .osr_p = BME280_OVERSAMPLING_1X,
    .osr_t = BME280_OVERSAMPLING_1X,
    .osr_h = BME280_OVERSAMPLING_1X,
    .filter = BME280_FILTER_COEFF_OFF,
};

static struct bme280_dev sensor;
static bme280_port_t port;

static app_bme280_sample_t latest;

static uint32_t measurement_delay_us;
static uint32_t last_start;
static uint32_t poll_interval_ms;

static uint8_t failures;
static bool initialized;

/* Return a retry interval for temporary transport unavailability.
 * Zero means the result is not a shared-bus deferral. */
static uint32_t defer_interval_ms(ui2c_status_t status) {
    switch (status) {
        case UI2C_STATUS_BUSY:
        case UI2C_STATUS_BUS_BUSY:
        case UI2C_STATUS_ARBITRATION_LOST: return BME280_BUS_RETRY_INTERVAL_MS;

        case UI2C_STATUS_NOT_INITIALIZED: return BME280_RETRY_INTERVAL_MS;

        default: return 0U;
    }
}

static int8_t init_sensor(void) {
    port = (bme280_port_t){
        .bus = bsp_i2c_bus(),
        .address = BME280_I2C_ADDR_PRIM,
        .time_ms = bsp_time_ms,
    };

    bme280_port_bind(&sensor, &port);

    int8_t result = bme280_init(&sensor);

    /* Try the secondary address only if the primary
     * address is absent or does not contain a BME280. */
    if (result == BME280_E_DEV_NOT_FOUND || port.status == UI2C_STATUS_NACK) {
        port.address = BME280_I2C_ADDR_SEC;
        result = bme280_init(&sensor);
    }

    if (result == BME280_OK) {
        result = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &sensor);
    }

    if (result == BME280_OK) {
        result = bme280_cal_meas_delay(&measurement_delay_us, &settings);
    }

    if (result == BME280_OK) {
        initialized = true;
        failures = 0U;
    }

    return result;
}

static int8_t read_sample(struct bme280_data* data) {
    int8_t result = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &sensor);

    if (result != BME280_OK) {
        return result;
    }

    sensor.delay_us(measurement_delay_us, sensor.intf_ptr);

    return bme280_get_sensor_data(BME280_ALL, data, &sensor);
}

void app_bme280_process(void) {
    uint32_t now = bsp_time_ms();

    /*
     * Zero permits the first attempt immediately.
     * Subsequent attempts use the interval selected by
     * the previous outcome, independently of latest.ready.
     */
    if (poll_interval_ms != 0U && (uint32_t)(now - last_start) < poll_interval_ms) {
        return;
    }

    last_start = now;

    /*
     * Another local client owns the bus.
     * Do not enter Bosch SensorAPI or modify sensor state.
     */
    if (ui2c_busy(bsp_i2c_bus())) {
        poll_interval_ms = BME280_BUS_RETRY_INTERVAL_MS;
        return;
    }

    app_bme280_sample_t sample = {0};
    struct bme280_data data;

    int8_t result = BME280_OK;

    /* Initialize only when the sensor context is unavailable. */
    if (!initialized) {
        result = init_sensor();
    }

    if (result == BME280_OK) {
        result = read_sample(&data);
    }

    /*
     * The bus can become unavailable during a transaction
     * even when the initial availability check succeeds.
     */
    if (result != BME280_OK) {
        uint32_t delay = defer_interval_ms(port.status);

        if (delay != 0U) {
            last_start = bsp_time_ms();
            poll_interval_ms = delay;
            return;
        }
    }

    sample.sensor_status = result;
    sample.transport_status = port.status;
    sample.address = port.address;

    if (result == BME280_OK) {
        sample.temperature_centi_c = data.temperature;
        sample.pressure_centi_pa = data.pressure;
        sample.humidity_centi_percent = (data.humidity * 100U + 512U) / 1024U;

        failures = 0U;
    } else if (initialized && ++failures >= BME280_FAILURE_LIMIT) {
        /*
         * Only actual measurement failures can invalidate
         * the cached Bosch configuration and calibration.
         */
        initialized = false;
        failures = 0U;
    }

    poll_interval_ms = initialized ? BME280_SAMPLE_INTERVAL_MS : BME280_RETRY_INTERVAL_MS;

    sample.sampled_at_ms = bsp_time_ms();
    sample.ready = true;

    latest = sample;
}

const app_bme280_sample_t* app_bme280_latest(void) { return &latest; }
