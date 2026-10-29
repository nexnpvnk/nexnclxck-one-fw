#include "bsp_rtc.h"
#include <stddef.h>
#include "rtc.h"

#define BSP_RTC_YEAR_BASE 2000U
#define BSP_RTC_BKP_MAGIC 0x434C4B31U /* "CLK1" */

static uint8_t from_bcd(uint32_t value) { return (uint8_t)((value >> 4U) * 10U + (value & 15U)); }

static uint32_t to_bcd(uint32_t value) { return ((value / 10U) << 4U) | (value % 10U); }

bool bsp_rtc_read(bsp_rtc_datetime_t* datetime) {
    if (datetime == NULL || !rtc_is_ready()) {
        return false;
    }
    /* ES0202 2.11.2: retry if SSR changed during the shadow snapshot.
     * Every SSR/TR read is followed by DR to release the shadow lock. */
    for (unsigned attempt = 0U; attempt < 3U; ++attempt) {
        uint32_t subsecond = RTC->SSR;
        uint32_t time = RTC->TR;
        uint32_t date = RTC->DR;
        uint32_t after = RTC->SSR;
        (void)RTC->DR;
        if (subsecond != after) {
            continue;
        }
        datetime->year = BSP_RTC_YEAR_BASE + from_bcd((date >> 16U) & 0xFFU);
        datetime->month = from_bcd((date >> 8U) & 0x1FU);
        datetime->day = from_bcd(date & 0x3FU);
        datetime->weekday = (uint8_t)((date >> 13U) & 7U);
        datetime->hour = from_bcd((time >> 16U) & 0x3FU);
        datetime->minute = from_bcd((time >> 8U) & 0x7FU);
        datetime->second = from_bcd(time & 0x7FU);
        return true;
    }
    return false;
}

bool bsp_rtc_write(const bsp_rtc_datetime_t* datetime) {
    if (datetime == NULL || datetime->year < 2000U || datetime->year > 2099U || datetime->month < 1U
        || datetime->month > 12U || datetime->day < 1U || datetime->day > 31U || datetime->weekday < 1U
        || datetime->weekday > 7U || datetime->hour > 23U || datetime->minute > 59U || datetime->second > 59U) {
        return false;
    }
    PWR->CR |= PWR_CR_DBP;
    RTC->BKP0R = 0U;
    if (!rtc_begin_update()) {
        return false;
    }
    /* Date and time form one update; validity is published only after synchronization. */
    RTC->TR = (to_bcd(datetime->hour) << 16U) | (to_bcd(datetime->minute) << 8U) | to_bcd(datetime->second);
    RTC->DR = (to_bcd(datetime->year - BSP_RTC_YEAR_BASE) << 16U) | ((uint32_t)datetime->weekday << 13U)
              | (to_bcd(datetime->month) << 8U) | to_bcd(datetime->day);
    RTC->CR |= RTC_CR_COE | RTC_CR_COSEL;
    if (!rtc_end_update()) {
        return false;
    }
    RTC->BKP0R = BSP_RTC_BKP_MAGIC;
    return true;
}

bool bsp_rtc_is_valid(void) { return rtc_is_ready() && RTC->BKP0R == BSP_RTC_BKP_MAGIC; }

bool bsp_rtc_invalidate(void) {
    PWR->CR |= PWR_CR_DBP;
    RTC->BKP0R = 0U;
    return RTC->BKP0R == 0U;
}

bool bsp_rtc_get_status(bsp_rtc_status_t* status) {
    if (status == NULL) {
        return false;
    }
    status->ready = rtc_is_ready();
    status->source_ready = (RCC->BDCR & RCC_BDCR_LSERDY) != 0U;
    return true;
}
