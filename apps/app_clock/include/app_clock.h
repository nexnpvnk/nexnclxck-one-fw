#ifndef APP_CLOCK_H
#define APP_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_CLOCK_WEEKDAY_MONDAY = 1,
    APP_CLOCK_WEEKDAY_TUESDAY,
    APP_CLOCK_WEEKDAY_WEDNESDAY,
    APP_CLOCK_WEEKDAY_THURSDAY,
    APP_CLOCK_WEEKDAY_FRIDAY,
    APP_CLOCK_WEEKDAY_SATURDAY,
    APP_CLOCK_WEEKDAY_SUNDAY,
} app_clock_weekday_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    app_clock_weekday_t weekday;

    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} app_clock_datetime_t;

typedef struct {
    bool rtc_ready;
    bool lse_ready;
    bool datetime_valid;
} app_clock_status_t;

bool app_clock_get(app_clock_datetime_t* datetime);
bool app_clock_set(const app_clock_datetime_t* datetime);

bool app_clock_is_valid(void);
bool app_clock_invalidate(void);

bool app_clock_get_status(app_clock_status_t* status);

#ifdef __cplusplus
}
#endif

#endif /* APP_CLOCK_H */
