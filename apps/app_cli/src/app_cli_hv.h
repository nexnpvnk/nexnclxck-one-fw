#ifndef APP_CLI_HV_H
#define APP_CLI_HV_H

#include "wsh_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const WshShellCmd_t app_cli_hv_cmd;

void app_cli_hv_process(void);
void app_cli_hv_cancel(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CLI_HV_H */
