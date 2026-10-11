#ifndef APP_CLI_BME280_H
#define APP_CLI_BME280_H
#include "wsh_shell.h"
extern const WshShellCmd_t app_cli_bme280_cmd;
void app_cli_bme280_process(void);
void app_cli_bme280_cancel(void);
#endif
