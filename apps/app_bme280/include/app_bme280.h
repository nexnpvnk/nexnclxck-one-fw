#ifndef APP_BME280_H
#define APP_BME280_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t temperature_centi_c;
    uint32_t pressure_centi_pa;
    uint32_t humidity_centi_percent;
} app_bme280_readings_t;

/* Foreground only. First call reads immediately; normal polling interval is 100 ms.
 * Blocking, with no catch-up reads for missed periods. */
void app_bme280_process(void);

/* Foreground only. Copies saved readings without I/O. An acquisition error makes
 * them unavailable until the next success; deferring an unstarted read keeps them.
 * Returns false before the first success, after an error or for NULL.
 * On false, out remains unchanged. Readings do not expire by age. */
bool app_bme280_get(app_bme280_readings_t* out);

#ifdef __cplusplus
}
#endif

#endif /* APP_BME280_H */
