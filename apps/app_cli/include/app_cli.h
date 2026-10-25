#ifndef APP_CLI_H
#define APP_CLI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool app_cli_init(void);
void app_cli_process(void);

bool app_cli_parse_u32(const char* text, uint32_t max, bool allow_hex, uint32_t* value);

#ifdef __cplusplus
}
#endif

#endif /* APP_CLI_H */
