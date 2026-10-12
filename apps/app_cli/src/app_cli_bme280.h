#ifndef APP_CLI_BME280_H
#define APP_CLI_BME280_H

#include "wsh_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const WshShellCmd_t app_cli_bme280_cmd;

void app_cli_bme280_process(void);
void app_cli_bme280_cancel(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CLI_BME280_H */
