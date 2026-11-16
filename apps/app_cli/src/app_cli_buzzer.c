#include "app_cli_buzzer.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_cli.h"
#include "app_cli_args.h"
#include "app_sound.h"
#include "bsp_buzzer.h"
#include "wsh_shell.h"

enum { OPT_DEFAULT, OPT_HELP, OPT_FREQUENCY, OPT_VOLUME, OPT_END };

static const WshShellOption_t read_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show buzzer status")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t control_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Enable or disable buzzer")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t set_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Set buzzer parameters (either or both)")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_FREQUENCY, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-f", "--frequency", "Frequency in Hz (100..10000)")},
    {OPT_VOLUME, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-v", "--volume", "Volume (0..100%)")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static WSH_SHELL_RET_STATE_t print_status(void) {
    bsp_buzzer_status_t status;

    if (!bsp_buzzer_get_status(&status)) {
        WSH_SHELL_PRINT_ERR("Buzzer unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_SYS(
        "State     : %s\n"
        "Frequency : %lu Hz\n"
        "Volume    : %u %%\n",
        status.enabled ? "on" : "off",
        (unsigned long)status.frequency_hz,
        (unsigned)status.volume_percent
    );

    return WSH_SHELL_RET_STATE_SUCCESS;
}

/**
 * \brief           Show buzzer status after collecting the command arguments
 * \param[in]       cmd: Command descriptor
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument vector
 * \param[in]       ctx: Shell context
 * \return          Shell command result
 */
static WSH_SHELL_RET_STATE_t read_handler(
    const WshShellCmd_t* cmd,
    WshShell_Size_t argc,
    const WshShell_Char_t* argv[],
    void* ctx
) {
    app_cli_args_result_t result = app_cli_collect_args(cmd, argc, argv, ctx, NULL, 0U);

    if (result != APP_CLI_ARGS_READY) {
        return result == APP_CLI_ARGS_HELP ? WSH_SHELL_RET_STATE_SUCCESS : WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    return print_status();
}

/**
 * \brief           Validate requested buzzer parameters before taking manual control
 * \param[in]       cmd: Command descriptor
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument vector
 * \param[in]       ctx: Shell context
 * \return          Shell command result
 */
static WSH_SHELL_RET_STATE_t set_handler(
    const WshShellCmd_t* cmd,
    WshShell_Size_t argc,
    const WshShell_Char_t* argv[],
    void* ctx
) {
    app_cli_args_result_t result;
    uint32_t frequency = 0U;
    uint32_t volume = 0U;
    const char* values[OPT_END];
    const char* frequency_text;
    const char* volume_text;
    bool has_frequency;
    bool has_volume;

    result = app_cli_collect_args(cmd, argc, argv, ctx, values, WSH_SHELL_ARR_LEN(values));
    if (result != APP_CLI_ARGS_READY) {
        return result == APP_CLI_ARGS_HELP ? WSH_SHELL_RET_STATE_SUCCESS : WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    frequency_text = values[OPT_FREQUENCY];
    volume_text = values[OPT_VOLUME];
    has_frequency = frequency_text != NULL;
    has_volume = volume_text != NULL;
    if (!has_frequency && !has_volume) {
        WSH_SHELL_PRINT_WARN("Specify --frequency or --volume (or both); see buzzer set --help\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (has_frequency && !app_cli_parse_u32(frequency_text, UINT32_MAX, false, &frequency)) {
        WSH_SHELL_PRINT_WARN("Invalid frequency: expected an integer (100..10000 Hz)\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (has_volume && !app_cli_parse_u32(volume_text, UINT32_MAX, false, &volume)) {
        WSH_SHELL_PRINT_WARN("Invalid volume: expected an integer (0..100%%)\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (has_frequency && (frequency < BSP_BUZZER_FREQ_MIN_HZ || frequency > BSP_BUZZER_FREQ_MAX_HZ)) {
        WSH_SHELL_PRINT_WARN("Frequency must be 100..10000 Hz\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (has_volume && volume > BSP_BUZZER_VOLUME_MAX_PERCENT) {
        WSH_SHELL_PRINT_WARN("Volume must be 0..100%%\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    /* All arguments are valid before manual control takes ownership. */
    app_sound_cancel();
    if (has_frequency && !bsp_buzzer_set_frequency(frequency)) {
        WSH_SHELL_PRINT_ERR("Failed to set buzzer frequency\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    if (has_volume && !bsp_buzzer_set_volume((uint8_t)volume)) {
        WSH_SHELL_PRINT_ERR("Failed to set buzzer volume\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_INFO("Buzzer parameters updated\n");
    return print_status();
}

/**
 * \brief           Enable or disable manual buzzer output after validating command syntax
 * \param[in]       cmd: Command descriptor
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument vector
 * \param[in]       ctx: Shell context
 * \param[in]       enabled: Requested buzzer state
 * \return          Shell command result
 */
static WSH_SHELL_RET_STATE_t control_handler(
    const WshShellCmd_t* cmd,
    WshShell_Size_t argc,
    const WshShell_Char_t* argv[],
    void* ctx,
    bool enabled
) {
    app_cli_args_result_t result = app_cli_collect_args(cmd, argc, argv, ctx, NULL, 0U);

    if (result != APP_CLI_ARGS_READY) {
        return result == APP_CLI_ARGS_HELP ? WSH_SHELL_RET_STATE_SUCCESS : WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    app_sound_cancel();
    if (!bsp_buzzer_set_enabled(enabled)) {
        WSH_SHELL_PRINT_ERR("Buzzer unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_INFO("Buzzer %s\n", enabled ? "enabled" : "disabled");
    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t on_handler(
    const WshShellCmd_t* cmd,
    WshShell_Size_t argc,
    const WshShell_Char_t* argv[],
    void* ctx
) {
    return control_handler(cmd, argc, argv, ctx, true);
}

static WSH_SHELL_RET_STATE_t off_handler(
    const WshShellCmd_t* cmd,
    WshShell_Size_t argc,
    const WshShell_Char_t* argv[],
    void* ctx
) {
    return control_handler(cmd, argc, argv, ctx, false);
}

static const WshShellCmd_t on_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "on",
    .Descr = "Enable buzzer",
    .Options = control_opts,
    .OptNum = WSH_SHELL_ARR_LEN(control_opts),
    .Handler = on_handler,
};

static const WshShellCmd_t off_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "off",
    .Descr = "Disable buzzer",
    .Options = control_opts,
    .OptNum = WSH_SHELL_ARR_LEN(control_opts),
    .Handler = off_handler,
};

static const WshShellCmd_t set_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "set",
    .Descr = "Set buzzer parameters: buzzer set [-f <Hz>] [-v <percent>] (either or both)",
    .Options = set_opts,
    .OptNum = WSH_SHELL_ARR_LEN(set_opts),
    .Handler = set_handler,
};

static const WshShellCmd_t* const subcommands[] = {
    &on_cmd,
    &off_cmd,
    &set_cmd,
};

const WshShellCmd_t app_cli_buzzer_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "buzzer",
    .Descr = "Buzzer control",
    .Options = read_opts,
    .OptNum = WSH_SHELL_ARR_LEN(read_opts),
    .Handler = read_handler,
    .SubCmds = subcommands,
    .SubCmdNum = WSH_SHELL_ARR_LEN(subcommands),
};
