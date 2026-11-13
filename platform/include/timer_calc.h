#ifndef TIMER_CALC_H
#define TIMER_CALC_H

#include <stdint.h>

/* Pure arithmetic for edge-aligned upcounters; no peripheral or driver dependency.
 * PSC rounds to the nearest divisor; ARR truncates the period. Validate
 * exactness and register widths at each use site. Arguments must be positive 32-bit frequencies,
 * clock_hz >= counter_hz and clock_hz / (psc + 1) >= rate_hz.
 * Arguments may be evaluated more than once; do not pass side effects.
 * Keep results wide so range checks cannot be defeated by a narrowing cast. */
#define TIMER_CALC_PSC(clock_hz, counter_hz)                                                                           \
    (((uint64_t)(clock_hz) + (uint64_t)(counter_hz) / 2U) / (uint64_t)(counter_hz) - 1U)

#define TIMER_CALC_ARR(clock_hz, psc, rate_hz)                                                                         \
    ((uint64_t)(clock_hz) / (((uint64_t)(psc) + 1U) * (uint64_t)(rate_hz)) - 1U)

#define TIMER_CALC_RATE_HZ(clock_hz, psc, arr)                                                                         \
    ((uint64_t)(clock_hz) / (((uint64_t)(psc) + 1U) * ((uint64_t)(arr) + 1U)))

#endif /* TIMER_CALC_H */
