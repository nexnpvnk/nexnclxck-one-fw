#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t frequency_hz;
    bool enabled;
} bsp_buzzer_status_t;

bool bsp_buzzer_init(void);

bool bsp_buzzer_set_frequency(uint32_t frequency_hz);
bool bsp_buzzer_set_enabled(bool enabled);

bool bsp_buzzer_get_status(bsp_buzzer_status_t* status);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BUZZER_H */
