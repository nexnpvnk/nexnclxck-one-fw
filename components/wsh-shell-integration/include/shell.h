#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>

#include "wsh_shell_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

bool shell_init(void);
void shell_process(void);

/* Foreground callback before normal Ctrl+C handling; NULL unregisters it. */
void shell_set_cancel_handler(void (*handler)(void));

bool shell_commands_attach(
    const WshShellCmd_t* commands[],
    WshShell_Size_t count);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_H */
