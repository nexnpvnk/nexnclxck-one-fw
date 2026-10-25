#ifndef APP_INPUT_H
#define APP_INPUT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool app_input_init(void);
void app_input_process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INPUT_H */
