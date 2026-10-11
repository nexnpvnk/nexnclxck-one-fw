#include "app_bme280.h"
#include "bme280_port.h"
#include "bsp_i2c.h"
#include "bsp_time.h"

static app_bme280_sample_t latest;
static uint32_t last_start;

/* Bosch owns initialization, calibration decoding and compensation. Reinitialize
 * for each sample so a replaced/reset device needs no separate recovery state. */
static int8_t read_sample(struct bme280_data* data, app_bme280_sample_t* sample) {
    struct bme280_dev sensor;
    bme280_port_t port;
    port = (bme280_port_t){.bus = bsp_i2c_bus(), .address = 0x76, .time_ms = bsp_time_ms};
    bme280_port_bind(&sensor, &port);
    int8_t result = bme280_init(&sensor);
    if (result == BME280_E_DEV_NOT_FOUND || port.status == UI2C_STATUS_NACK) {
        port.address = 0x77;
        result = bme280_init(&sensor);
    }
    const struct bme280_settings settings = {
        .osr_p = BME280_OVERSAMPLING_1X,
        .osr_t = BME280_OVERSAMPLING_1X,
        .osr_h = BME280_OVERSAMPLING_1X,
        .filter = BME280_FILTER_COEFF_OFF,
    };
    if (result == BME280_OK) {
        result = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &sensor);
    }
    uint32_t delay = 0;
    if (result == BME280_OK) {
        result = bme280_cal_meas_delay(&delay, &settings);
    }
    if (result == BME280_OK) {
        result = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &sensor);
    }
    if (result == BME280_OK) {
        sensor.delay_us(delay, sensor.intf_ptr);
        result = bme280_get_sensor_data(BME280_ALL, data, &sensor);
    }
    sample->address = port.address;
    sample->transport_status = port.status;
    return result;
}

void app_bme280_process(void) {
    uint32_t now = bsp_time_ms();
    if (latest.ready && (uint32_t)(now - last_start) < 500U) {
        return;
    }
    last_start = now;
    app_bme280_sample_t sample = {0};
    struct bme280_data data;
    sample.sensor_status = read_sample(&data, &sample);
    if (sample.sensor_status == BME280_OK) {
        sample.temperature_centi_c = data.temperature;
        sample.pressure_centi_pa = data.pressure;
        sample.humidity_centi_percent = (data.humidity * 100U + 512U) / 1024U;
    }
    sample.sampled_at_ms = bsp_time_ms();
    sample.ready = true;
    latest = sample;
}

const app_bme280_sample_t* app_bme280_latest(void) { return &latest; }
