/**
 * \file            app_cli_args.h
 * \brief           Shared argument collection policy for application commands
 */

/* SPDX-License-Identifier: Apache-2.0 */

#ifndef APP_CLI_ARGS_H
#define APP_CLI_ARGS_H

#include <stddef.h>

#include "wsh_shell_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Outcome of collecting command arguments
 */
typedef enum {
    APP_CLI_ARGS_READY, /* Arguments collected; the handler may validate and apply them. */
    APP_CLI_ARGS_HELP,  /* Help printed; the handler must return success without applying changes. */
    APP_CLI_ARGS_ERROR, /* Invalid input; the handler must return a parameter error. */
} app_cli_args_result_t;

/* Values are borrowed argv pointers indexed by option ID, valid only on READY.
 * Absent options are NULL; flags point to their token, strings to their value.
 * Pass NULL and zero capacity for commands accepting only the default and help. */
app_cli_args_result_t app_cli_collect_args(
    const WshShellCmd_t* cmd,
    WshShell_Size_t argc,
    const WshShell_Char_t* argv[],
    void* ctx,
    const char* values[],
    size_t value_count
);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APP_CLI_ARGS_H */
