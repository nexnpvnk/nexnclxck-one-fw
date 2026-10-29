/**
  ******************************************************************************
  * @file    rtc.h
  * @brief   This file contains all the function prototypes for
  *          the rtc.c file
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
#ifndef __RTC_H__
#define __RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "main.h"

void platform_rtc_init(void);
/* Foreground only; SysTick interrupts must remain enabled. */
bool rtc_begin_update(void);
bool rtc_end_update(void);
bool rtc_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* __RTC_H__ */
