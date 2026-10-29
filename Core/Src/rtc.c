/**
  ******************************************************************************
  * @file    rtc.c
  * @brief   This file provides code for the configuration
  *          of the RTC instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "rtc.h"
#include "bsp_time.h"

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

bool rtc_begin_update(void) {
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

bool rtc_end_update(void) {
    RTC->ISR &= ~RTC_ISR_INIT;
    ready = sync_shadow();
    RTC->WPR = 0xFFU;
    return ready;
}

bool rtc_is_ready(void) { return ready; }

void platform_rtc_init(void) {
    if (!rtc_begin_update()) {
        Error_Handler();
    }
    RTC->CR = (RTC->CR & ~(RTC_CR_FMT | RTC_CR_OSEL | RTC_CR_POL | RTC_CR_BYPSHAD)) | RTC_CR_COE | RTC_CR_COSEL;
    RTC->PRER = 255U;
    RTC->PRER |= 127U << 16U;
    RTC->TAFCR &= ~RTC_TAFCR_ALARMOUTTYPE;
    if (!rtc_end_update()) {
        Error_Handler();
    }
}
