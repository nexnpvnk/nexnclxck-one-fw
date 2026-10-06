#include "app_clock.h"

#include <stddef.h>

#include "rtc.h"
#include "stm32f0xx_hal_rcc.h"

#define APP_CLOCK_YEAR_MIN     2000U
#define APP_CLOCK_YEAR_MAX     2099U
#define APP_CLOCK_YEAR_BASE    2000U

#define APP_CLOCK_BKP_REGISTER RTC_BKP_DR0
#define APP_CLOCK_BKP_MAGIC    0x434C4B31U /* "CLK1" */

static bool app_clock_is_leap_year(uint16_t year) {
    return ((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U));
}

static uint8_t app_clock_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {
        31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U,
    };

    if ((month < 1U) || (month > 12U)) {
        return 0U;
    }

    if ((month == 2U) && app_clock_is_leap_year(year)) {
        return 29U;
    }

    return days[month - 1U];
}

static bool app_clock_datetime_is_valid(const app_clock_datetime_t* datetime) {
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

    days = app_clock_days_in_month(datetime->year, datetime->month);

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

static app_clock_weekday_t app_clock_weekday_calc(uint16_t year, uint8_t month, uint8_t day) {
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
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    if (datetime == NULL) {
        return false;
    }

    /*
     * STM32 RTC shadow registers must be read in this order:
     * time first, date second.
     */
    if (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) {
        return false;
    }

    if (HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
        return false;
    }

    datetime->year = APP_CLOCK_YEAR_BASE + date.Year;
    datetime->month = date.Month;
    datetime->day = date.Date;
    datetime->weekday = (app_clock_weekday_t)date.WeekDay;

    datetime->hour = time.Hours;
    datetime->minute = time.Minutes;
    datetime->second = time.Seconds;

    return true;
}

bool app_clock_is_valid(void) { return HAL_RTCEx_BKUPRead(&hrtc, APP_CLOCK_BKP_REGISTER) == APP_CLOCK_BKP_MAGIC; }

bool app_clock_get(app_clock_datetime_t* datetime) {
    if (!app_clock_is_valid()) {
        return false;
    }

    return app_clock_read(datetime);
}

static bool app_clock_sync(void) {
    HAL_StatusTypeDef status;

    /*
     * STM32F051 errata ES0202 2.11.4:
     * before entering RTC initialization mode again,
     * BYPSHAD must be cleared and RSF synchronization completed.
     */
    if (HAL_RTCEx_DisableBypassShadow(&hrtc) != HAL_OK) {
        return false;
    }

    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);

    status = HAL_RTC_WaitForSynchro(&hrtc);

    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);

    return status == HAL_OK;
}

bool app_clock_set(const app_clock_datetime_t* datetime) {
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    if (!app_clock_datetime_is_valid(datetime)) {
        return false;
    }

    time.Hours = datetime->hour;
    time.Minutes = datetime->minute;
    time.Seconds = datetime->second;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;

    date.Year = (uint8_t)(datetime->year - APP_CLOCK_YEAR_BASE);

    date.Month = datetime->month;
    date.Date = datetime->day;

    date.WeekDay = (uint8_t)app_clock_weekday_calc(datetime->year, datetime->month, datetime->day);

    /*
     * SystemClock_Config() already enables backup-domain access.
     * Keep it enabled during normal firmware operation.
     */
    HAL_PWR_EnableBkUpAccess();

    /*
     * Mark the calendar invalid until the whole update succeeds.
     */
    HAL_RTCEx_BKUPWrite(&hrtc, APP_CLOCK_BKP_REGISTER, 0U);

    if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) {
        return false;
    }

    /*
     * Required for STM32F051 RTC errata ES0202 2.11.4.
     */
    if (!app_clock_sync()) {
        return false;
    }

    if (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
        return false;
    }

    if (!app_clock_sync()) {
        return false;
    }

    HAL_RTCEx_BKUPWrite(&hrtc, APP_CLOCK_BKP_REGISTER, APP_CLOCK_BKP_MAGIC);

    return true;
}

bool app_clock_invalidate(void) {
    HAL_PWR_EnableBkUpAccess();

    HAL_RTCEx_BKUPWrite(&hrtc, APP_CLOCK_BKP_REGISTER, 0U);

    return !app_clock_is_valid();
}

bool app_clock_get_status(app_clock_status_t* status) {
    if (status == NULL) {
        return false;
    }

    status->rtc_ready = HAL_RTC_GetState(&hrtc) == HAL_RTC_STATE_READY;

    status->lse_ready = __HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) != RESET;

    status->datetime_valid = app_clock_is_valid();

    return true;
}
