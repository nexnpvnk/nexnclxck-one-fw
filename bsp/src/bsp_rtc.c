#include "bsp_rtc.h"

#include "rtc.h"
#include "stm32f0xx_hal_rcc.h"

#define BSP_RTC_YEAR_BASE    2000U

#define BSP_RTC_BKP_REGISTER RTC_BKP_DR0
#define BSP_RTC_BKP_MAGIC    0x434C4B31U /* "CLK1" */

static bool sync(void) {
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

bool bsp_rtc_read(bsp_rtc_datetime_t* datetime) {
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

    datetime->year = BSP_RTC_YEAR_BASE + date.Year;
    datetime->month = date.Month;
    datetime->day = date.Date;
    datetime->weekday = date.WeekDay;

    datetime->hour = time.Hours;
    datetime->minute = time.Minutes;
    datetime->second = time.Seconds;

    return true;
}

bool bsp_rtc_write(const bsp_rtc_datetime_t* datetime) {
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    if (datetime == NULL) {
        return false;
    }

    time.Hours = datetime->hour;
    time.Minutes = datetime->minute;
    time.Seconds = datetime->second;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;

    date.Year = (uint8_t)(datetime->year - BSP_RTC_YEAR_BASE);

    date.Month = datetime->month;
    date.Date = datetime->day;

    date.WeekDay = datetime->weekday;

    /*
     * SystemClock_Config() already enables backup-domain access.
     * Keep it enabled during normal firmware operation.
     */
    HAL_PWR_EnableBkUpAccess();

    /*
     * Mark the calendar invalid until the whole update succeeds.
     */
    HAL_RTCEx_BKUPWrite(&hrtc, BSP_RTC_BKP_REGISTER, 0U);

    if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) {
        return false;
    }

    /*
     * Required for STM32F051 RTC errata ES0202 2.11.4.
     */
    if (!sync()) {
        return false;
    }

    if (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
        return false;
    }

    if (!sync()) {
        return false;
    }

    if (HAL_RTCEx_SetCalibrationOutPut(&hrtc, RTC_CALIBOUTPUT_1HZ) != HAL_OK) {
        return false;
    }

    HAL_RTCEx_BKUPWrite(&hrtc, BSP_RTC_BKP_REGISTER, BSP_RTC_BKP_MAGIC);

    return true;
}

bool bsp_rtc_is_valid(void) { return HAL_RTCEx_BKUPRead(&hrtc, BSP_RTC_BKP_REGISTER) == BSP_RTC_BKP_MAGIC; }

bool bsp_rtc_invalidate(void) {
    HAL_PWR_EnableBkUpAccess();

    HAL_RTCEx_BKUPWrite(&hrtc, BSP_RTC_BKP_REGISTER, 0U);

    return !bsp_rtc_is_valid();
}

bool bsp_rtc_get_status(bsp_rtc_status_t* status) {
    if (status == NULL) {
        return false;
    }

    status->ready = HAL_RTC_GetState(&hrtc) == HAL_RTC_STATE_READY;
    status->source_ready = __HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) != RESET;
    return true;
}
