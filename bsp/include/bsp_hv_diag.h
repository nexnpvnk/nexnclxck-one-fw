#ifndef BSP_HV_DIAG_H
#define BSP_HV_DIAG_H

#include <stdint.h>

#ifdef HV_DIAGNOSTICS
enum { BSP_HV_TRACE_COUNT = 128 };

typedef struct {
    uint32_t filtered_mv;
    uint16_t raw_min;
    uint16_t raw_max;
    uint16_t compare;
    uint16_t pwm_phase;
    uint16_t flags; /* ADC ISR low byte; valid=8, hold=9, separators=10,
                    * target=(V-100)/10 in 11..14, enabled=15. */
    uint16_t irq_us;
} bsp_hv_trace_t;

/* SWD writes capture=1 to arm; IRQ sets 2 while recording and 3 when frozen.
 * Apply gain changes with HV off or close to regulation. Captures never print
 * or halt the CPU; the host restricts tuning steps to 170..180 V. */
typedef struct {
    uint32_t capture;
    uint32_t decimation;
    uint32_t count;
    uint32_t samples;
    uint32_t rejected;
    uint32_t ovp_holds;
    uint32_t overruns;
    uint32_t filtered_mv;
    uint32_t raw_min;
    uint32_t raw_max;
    uint32_t divider;
    uint32_t interval_flags;
    uint32_t irq_max_us;
    uint32_t kp_q16;
    uint32_t ki_q16;
    uint32_t reserved;
    uint32_t filter_shift;
    uint32_t target_v;
    bsp_hv_trace_t records[BSP_HV_TRACE_COUNT];
} bsp_hv_diag_t;

extern volatile bsp_hv_diag_t bsp_hv_diag;
#endif

#endif
