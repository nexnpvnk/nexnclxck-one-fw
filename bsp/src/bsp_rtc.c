#include "bsp_rtc.h"
#include "bsp_time.h"
#include "bsp_hw.h"
#include <stddef.h>

static bool ready;

static bool wait_flag(uint32_t flag) {
    uint32_t start = bsp_time_ms();
    while ((RTC->ISR & flag) == 0U) {
        if ((uint32_t)(bsp_time_ms() - start) >= 1000U) {
            return false;
        }
    }
    return true;
}

/* WPR must be unlocked. Writing ones preserves unrelated write-zero flags. */
static bool sync_shadow(void) {
    RTC->CR &= ~RTC_CR_BYPSHAD;
    RTC->ISR = ~(RTC_ISR_RSF | RTC_ISR_INIT);
    return wait_flag(RTC_ISR_RSF);
}

static bool rtc_begin_update(void) {
    if ((RCC->BDCR & (RCC_BDCR_RTCEN | RCC_BDCR_LSERDY)) != (RCC_BDCR_RTCEN | RCC_BDCR_LSERDY)) {
        ready = false;
        return false;
    }
    PWR->CR |= PWR_CR_DBP;
    RTC->WPR = 0xCAU;
    RTC->WPR = 0x53U;
    /* ES0202 2.11.4: synchronize shadow registers before re-entering INIT. */
    if ((RTC->ISR & RTC_ISR_INITS) != 0U && !sync_shadow()) {
        RTC->WPR = 0xFFU;
        ready = false;
        return false;
    }
    RTC->ISR |= RTC_ISR_INIT;
    if (!wait_flag(RTC_ISR_INITF)) {
        RTC->ISR &= ~RTC_ISR_INIT;
        RTC->WPR = 0xFFU;
        ready = false;
        return false;
    }
    return true;
}

static bool rtc_end_update(void) {
    RTC->ISR &= ~RTC_ISR_INIT;
    ready = sync_shadow();
    RTC->WPR = 0xFFU;
    return ready;
}

void bsp_rtc_hw_init(void) {
    /* Keep the digital input enabled for PC13 readback. RTC_CALIB overrides
     * the output driver independently of the GPIO mode. No pull on the LED. */
    GPIOC->MODER &= ~GPIO_MODER_MODER13;
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPDR13;
    if (!rtc_begin_update()) {
        Error_Handler();
    }
    RTC->CR = (RTC->CR & ~(RTC_CR_FMT | RTC_CR_OSEL | RTC_CR_POL | RTC_CR_BYPSHAD)) | RTC_CR_COE | RTC_CR_COSEL;
    RTC->PRER = 255U;
    RTC->PRER |= RTC_PRER_PREDIV_A;
    RTC->TAFCR &= ~RTC_TAFCR_ALARMOUTTYPE;
    if (!rtc_end_update()) {
        Error_Handler();
    }
}

#define BSP_RTC_YEAR_BASE 2000U
#define BSP_RTC_BKP_MAGIC 0x434C4B31U /* "CLK1" */

static uint8_t from_bcd(uint32_t value) { return (uint8_t)((value >> 4U) * 10U + (value & 15U)); }

static uint32_t to_bcd(uint32_t value) { return ((value / 10U) << 4U) | (value % 10U); }

bool bsp_rtc_read(bsp_rtc_datetime_t* datetime) {
    if (datetime == NULL || !ready) {
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
        datetime->year = BSP_RTC_YEAR_BASE + from_bcd((date & (RTC_DR_YT | RTC_DR_YU)) >> RTC_DR_YU_Pos);
        datetime->month = from_bcd((date & (RTC_DR_MT | RTC_DR_MU)) >> RTC_DR_MU_Pos);
        datetime->day = from_bcd((date & (RTC_DR_DT | RTC_DR_DU)) >> RTC_DR_DU_Pos);
        datetime->weekday = (uint8_t)((date & RTC_DR_WDU) >> RTC_DR_WDU_Pos);
        datetime->hour = from_bcd((time & (RTC_TR_HT | RTC_TR_HU)) >> RTC_TR_HU_Pos);
        datetime->minute = from_bcd((time & (RTC_TR_MNT | RTC_TR_MNU)) >> RTC_TR_MNU_Pos);
        datetime->second = from_bcd((time & (RTC_TR_ST | RTC_TR_SU)) >> RTC_TR_SU_Pos);
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
    RTC->TR = (to_bcd(datetime->hour) << RTC_TR_HU_Pos) | (to_bcd(datetime->minute) << RTC_TR_MNU_Pos)
              | (to_bcd(datetime->second) << RTC_TR_SU_Pos);
    RTC->DR = (to_bcd(datetime->year - BSP_RTC_YEAR_BASE) << RTC_DR_YU_Pos)
              | ((uint32_t)datetime->weekday << RTC_DR_WDU_Pos) | (to_bcd(datetime->month) << RTC_DR_MU_Pos)
              | (to_bcd(datetime->day) << RTC_DR_DU_Pos);
    RTC->CR |= RTC_CR_COE | RTC_CR_COSEL;
    if (!rtc_end_update()) {
        return false;
    }
    RTC->BKP0R = BSP_RTC_BKP_MAGIC;
    return true;
}

bool bsp_rtc_is_valid(void) { return ready && RTC->BKP0R == BSP_RTC_BKP_MAGIC; }

bool bsp_rtc_invalidate(void) {
    PWR->CR |= PWR_CR_DBP;
    RTC->BKP0R = 0U;
    return RTC->BKP0R == 0U;
}

bool bsp_rtc_get_status(bsp_rtc_status_t* status) {
    if (status == NULL) {
        return false;
    }
    status->ready = ready;
    status->source_ready = (RCC->BDCR & RCC_BDCR_LSERDY) != 0U;
    return true;
}
