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
int bsp_console_write(const void* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CONSOLE_H */
