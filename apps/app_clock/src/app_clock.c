#include "app_clock.h"
#include "bsp_rtc.h"

#include <stddef.h>

#define APP_CLOCK_YEAR_MIN  2000U
#define APP_CLOCK_YEAR_MAX  2099U

static bool is_leap_year(uint16_t year) {
    return ((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U));
}

static uint8_t days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {
        31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U,
    };

    if ((month < 1U) || (month > 12U)) {
        return 0U;
    }

    if ((month == 2U) && is_leap_year(year)) {
        return 29U;
    }

    return days[month - 1U];
}

static bool datetime_is_valid(const app_clock_datetime_t* datetime) {
    uint8_t days;

    if (datetime == NULL) {
        return false;
    }

    if ((datetime->year < APP_CLOCK_YEAR_MIN) || (datetime->year > APP_CLOCK_YEAR_MAX)) {
        return false;
    }

    if ((datetime->month < 1U) || (datetime->month > 12U)) {
        return false;
    }

    days = days_in_month(datetime->year, datetime->month);

    if ((datetime->day < 1U) || (datetime->day > days)) {
        return false;
    }

    if (datetime->hour > 23U) {
        return false;
    }

    if (datetime->minute > 59U) {
        return false;
    }

    if (datetime->second > 59U) {
        return false;
    }

    return true;
}

static app_clock_weekday_t weekday_calc(uint16_t year, uint8_t month, uint8_t day) {
    static const uint8_t offsets[] = {
        0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U,
    };

    uint16_t y;
    uint8_t weekday;

    y = year;

    if (month < 3U) {
        --y;
    }

    weekday = (uint8_t)((y + (y / 4U) - (y / 100U) + (y / 400U) + offsets[month - 1U] + day) % 7U);

    if (weekday == 0U) {
        return APP_CLOCK_WEEKDAY_SUNDAY;
    }

    return (app_clock_weekday_t)weekday;
}

static bool app_clock_read(app_clock_datetime_t* datetime) {
    bsp_rtc_datetime_t bsp_rtc_datetime;

    if (datetime == NULL) {
        return false;
    }

    if (!bsp_rtc_read(&bsp_rtc_datetime)) {
        return false;
    }

    datetime->year = bsp_rtc_datetime.year;
    datetime->month = bsp_rtc_datetime.month;
    datetime->day = bsp_rtc_datetime.day;
    datetime->weekday = (app_clock_weekday_t)bsp_rtc_datetime.weekday;

    datetime->hour = bsp_rtc_datetime.hour;
    datetime->minute = bsp_rtc_datetime.minute;
    datetime->second = bsp_rtc_datetime.second;

    return true;
}

bool app_clock_is_valid(void) { return bsp_rtc_is_valid(); }

bool app_clock_get(app_clock_datetime_t* datetime) {
    if (!app_clock_is_valid()) {
        return false;
    }

    return app_clock_read(datetime);
}

bool app_clock_set(const app_clock_datetime_t* datetime) {
    bsp_rtc_datetime_t bsp_rtc_datetime;

    if (!datetime_is_valid(datetime)) {
        return false;
    }

    bsp_rtc_datetime.hour = datetime->hour;
    bsp_rtc_datetime.minute = datetime->minute;
    bsp_rtc_datetime.second = datetime->second;

    bsp_rtc_datetime.year = datetime->year;

    bsp_rtc_datetime.month = datetime->month;
    bsp_rtc_datetime.day = datetime->day;

    bsp_rtc_datetime.weekday = (uint8_t)weekday_calc(datetime->year, datetime->month, datetime->day);

    return bsp_rtc_write(&bsp_rtc_datetime);
}

bool app_clock_invalidate(void) { return bsp_rtc_invalidate(); }

bool app_clock_get_status(app_clock_status_t* status) {
    bsp_rtc_status_t bsp_rtc_status;

    if (status == NULL) {
        return false;
    }

    if (!bsp_rtc_get_status(&bsp_rtc_status)) {
        return false;
    }

    status->rtc_ready = bsp_rtc_status.ready;

    status->source_ready = bsp_rtc_status.source_ready;

    status->datetime_valid = app_clock_is_valid();

    return true;
}
