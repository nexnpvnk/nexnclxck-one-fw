#ifndef APP_CLI_H
#define APP_CLI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool app_cli_init(void);
void app_cli_process(void);

/* One active application monitor; ctx is the shell context from its handler.
 * The command must prepare its initial snapshot before switching monitors. */
typedef enum { APP_CLI_WATCH_BME280, APP_CLI_WATCH_HV } app_cli_watch_t;

void app_cli_watch_start(app_cli_watch_t source, void* ctx);

/* Strict unsigned conversion: rejects suffixes and overflow, unlike shell INT. */
bool app_cli_parse_u32(const char* text, uint32_t max, bool allow_hex, uint32_t* value);

#ifdef __cplusplus
}
#endif

#endif /* APP_CLI_H */
