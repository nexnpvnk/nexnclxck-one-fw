#ifndef BSP_CONSOLE_H
#define BSP_CONSOLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int bsp_console_write(const void* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CONSOLE_H */
