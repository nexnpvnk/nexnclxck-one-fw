#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool shell_init(void);
void shell_process(void);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_H */
