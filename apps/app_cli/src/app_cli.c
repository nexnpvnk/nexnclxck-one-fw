#include "app_cli.h"

#include "app_cli_clock.h"
#include "shell.h"
#include "wsh_shell.h"

static const WshShellCmd_t* app_cli_commands[] = {
    &app_cli_clock_cmd,
};

bool app_cli_init(void) { return shell_commands_attach(app_cli_commands, WSH_SHELL_ARR_LEN(app_cli_commands)); }
