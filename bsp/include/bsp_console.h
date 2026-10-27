#ifndef BSP_CONSOLE_H
#define BSP_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool bsp_console_init(void);
void bsp_console_process(void);

int bsp_console_read(void* data, size_t len);

/* Foreground only, with interrupts and HAL tick enabled. Blocks while feeding
 * the 64-byte TX ring (63 usable bytes) and until UART transmission completes.
 * Returns len on success, 0 for an empty write, or -1 for invalid context/input
 * or a stalled transfer. An error can follow a partially transmitted message;
 * do not blindly retry the entire message. */
int bsp_console_write(const void* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CONSOLE_H */
