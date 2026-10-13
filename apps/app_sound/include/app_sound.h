#ifndef APP_SOUND_H
#define APP_SOUND_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_SOUND_MAX_DURATION_MS 60000U

/* Play a single nonblocking tone.
 * A new call replaces the currently active tone.
 * Returns false if the buzzer is unavailable, manually
 * enabled, or the arguments are invalid. */
bool app_sound_beep(uint32_t frequency_hz, uint32_t duration_ms);

/* Cancel only an active application-generated tone.
 * Does not affect a manually enabled buzzer. */
void app_sound_cancel(void);

/* Call regularly from the foreground main loop. */
void app_sound_process(void);

#ifdef __cplusplus
}
#endif


#endif /* APP_SOUND_H */
