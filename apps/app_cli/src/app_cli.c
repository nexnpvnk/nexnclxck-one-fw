#include "app_cli.h"

#include "app_cli_bme280.h"
#include "app_cli_clock.h"
#include "app_cli_i2c.h"
#include "shell.h"
#include "wsh_shell.h"

static const WshShellCmd_t* app_cli_commands[] = {
    &app_cli_clock_cmd,
    &app_cli_i2c_cmd,
    &app_cli_bme280_cmd,
};

bool app_cli_init(void) {
    if (!shell_commands_attach(app_cli_commands, WSH_SHELL_ARR_LEN(app_cli_commands))) {
        return false;
    }
    shell_set_cancel_handler(app_cli_bme280_cancel);
    return true;
}

void app_cli_process(void) { app_cli_bme280_process(); }
