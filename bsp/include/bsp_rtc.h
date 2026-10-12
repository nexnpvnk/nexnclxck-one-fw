#ifndef BSP_RTC_H
#define BSP_RTC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} bsp_rtc_datetime_t;

typedef struct {
    bool ready;
    bool source_ready;
} bsp_rtc_status_t;

bool bsp_rtc_read(bsp_rtc_datetime_t* datetime);
bool bsp_rtc_write(const bsp_rtc_datetime_t* datetime);

bool bsp_rtc_is_valid(void);
bool bsp_rtc_invalidate(void);

bool bsp_rtc_get_status(bsp_rtc_status_t* status);

#ifdef __cplusplus
}
#endif

#endif /* BSP_RTC_H */
