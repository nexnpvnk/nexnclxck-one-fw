#ifndef APP_BME280_H
#define APP_BME280_H
#include "ui2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t temperature_centi_c;
    uint32_t pressure_centi_pa;
    uint32_t humidity_centi_percent;
    uint32_t sampled_at_ms;
    ui2c_status_t transport_status;
    int8_t sensor_status; /* Bosch result: zero means success. */
    uint8_t address;
    bool ready; /* At least one attempt completed; values valid only on success. */
} app_bme280_sample_t;

/* Foreground only. First call reads immediately, then at most every 500 ms.
 * Blocking, with no catch-up reads for missed periods. */
void app_bme280_process(void);

/* Read-only latest attempt. No I/O; valid until the next process call. */
const app_bme280_sample_t* app_bme280_latest(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BME280_H */
