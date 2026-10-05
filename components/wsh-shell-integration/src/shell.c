#include "shell.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_console.h"
#include "wsh_shell.h"

#define SHELL_DEVICE_NAME "nexnclxck"

static WshShell_t shell;

#if WSH_SHELL_HISTORY
static WshShellHistory_t shell_history;
#endif

static const WshShellUser_t shell_users[] = {
    {
        .Login = "admin",
        .Salt = "0123456789abcdef",
        .Hash = "475ecf47",
        .Groups = WSH_SHELL_CMD_GROUP_ALL,
        .Rights = WSH_SHELL_OPT_ACCESS_ANY | WSH_SHELL_OPT_ACCESS_ADMIN,
    },
};

#if WSH_SHELL_HISTORY
static WshShellHistory_t shell_history_read(void) { return shell_history; }

static void shell_history_write(WshShellHistory_t history) { shell_history = history; }
#endif

bool shell_init(void) {
    WSH_SHELL_RET_STATE_t ret;

    ret = WshShell_Init(&shell, SHELL_DEVICE_NAME, NULL, NULL);

    if (ret != WSH_SHELL_RET_STATE_SUCCESS) {
        return false;
    }

    ret = WshShellUser_Attach(&shell.Users, shell_users, WSH_SHELL_ARR_LEN(shell_users), NULL);

    if (ret != WSH_SHELL_RET_STATE_SUCCESS) {
        return false;
    }

#if WSH_SHELL_HISTORY
    WshShellHistory_Init(&shell.HistoryIO, shell_history_read, shell_history_write);
#endif

    return true;
}

void shell_process(void) {
    uint8_t data[32];
    int len;

    bsp_console_process();

    while ((len = bsp_console_read(data, sizeof(data))) > 0) {
        for (int i = 0; i < len; ++i) {
            WshShell_InsertChar(&shell, (WshShell_Char_t)data[i]);
        }
    }
}
