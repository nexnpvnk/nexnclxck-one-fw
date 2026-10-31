#ifndef APP_CLI_BME280_H
#define APP_CLI_BME280_H

#include "wsh_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const WshShellCmd_t app_cli_bme280_cmd;
bool app_cli_bme280_watch_print(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CLI_BME280_H */
