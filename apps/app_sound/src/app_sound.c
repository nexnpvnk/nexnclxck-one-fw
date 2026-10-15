#include "app_sound.h"

#include "bsp_buzzer.h"
#include "bsp_time.h"

static struct {
    bool active;
    uint32_t started_ms;
    uint32_t duration_ms;
    uint32_t previous_frequency_hz;
    uint8_t previous_volume_percent;
} sound;

bool app_sound_beep(uint32_t frequency_hz, uint8_t volume_percent, uint32_t duration_ms) {
    bsp_buzzer_status_t status;

    if (duration_ms == 0U ||
        duration_ms > APP_SOUND_MAX_DURATION_MS ||
        volume_percent > BSP_BUZZER_VOLUME_MAX_PERCENT ||
        !bsp_buzzer_get_status(&status)) {
        return false;
    }

    /* Respect manual CLI control. */
    if (!sound.active && status.enabled) {
        return false;
    }

    if (!bsp_buzzer_set_frequency(frequency_hz)) {
        return false;
    }

    if (!bsp_buzzer_set_volume(volume_percent)) {
        (void)bsp_buzzer_set_frequency(status.frequency_hz);
        return false;
    }

    if (!bsp_buzzer_set_enabled(true)) {
        (void)bsp_buzzer_set_volume(status.volume_percent);
        (void)bsp_buzzer_set_frequency(status.frequency_hz);
        return false;
    }

    /* Preserve the previous setting only for the first tone.
     * Replacing a tone must not overwrite the saved frequency. */
    if (!sound.active) {
        sound.previous_frequency_hz = status.frequency_hz;
        sound.previous_volume_percent = status.volume_percent;
    }

    sound.started_ms = bsp_time_ms();
    sound.duration_ms = duration_ms;
    sound.active = true;

    return true;
}

void app_sound_cancel(void) {
    if (!sound.active) {
        return;
    }

    if (!bsp_buzzer_set_enabled(false)) {
        return;
    }

    sound.active = false;

    /* Restore the frequency and volume configured before the tone. */
    (void)bsp_buzzer_set_frequency(sound.previous_frequency_hz);
    (void)bsp_buzzer_set_volume(sound.previous_volume_percent);
}

void app_sound_process(void) {
    if (sound.active &&
        (uint32_t)(bsp_time_ms() - sound.started_ms) >=
            sound.duration_ms) {
        app_sound_cancel();
    }
}
