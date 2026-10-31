#include "app_cli_buzzer.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_cli.h"
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

    WSH_SHELL_PRINT_SYS("State     : %s\n"
                        "Frequency : %lu Hz\n"
                        "Volume    : %u %%\n",
                        status.enabled ? "on" : "off", (unsigned long)status.frequency_hz,
                        (unsigned)status.volume_percent);

    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t read_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                          void* ctx) {
    if (cmd == NULL || argc == 0U || argv == NULL || ctx == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    if (!WshShell_IsAuth(shell)) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_Size_t pos = 0U;
    bool help = false;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;
            case OPT_HELP: help = true; break;
            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    if (help) {
        WshShellCmd_PrintOptionsOverview(cmd);
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    return print_status();
}

static WSH_SHELL_RET_STATE_t set_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                         void* ctx) {
    if (cmd == NULL || argc == 0U || argv == NULL || ctx == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    if (!WshShell_IsAuth(shell)) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_Size_t pos = 0U;
    bool help = false;
    const char* frequency_text = NULL;
    const char* volume_text = NULL;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;
            case OPT_HELP: help = true; break;
            case OPT_FREQUENCY:
                if (frequency_text != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify --frequency only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                frequency_text = argv[opt.TokenPos + 1U];
                break;
            case OPT_VOLUME:
                if (volume_text != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify --volume only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                volume_text = argv[opt.TokenPos + 1U];
                break;
            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    if (help) {
        WshShellCmd_PrintOptionsOverview(cmd);
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    bool has_frequency = frequency_text != NULL;
    bool has_volume = volume_text != NULL;
    uint32_t frequency = 0U;
    uint32_t volume = 0U;
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

static WSH_SHELL_RET_STATE_t control_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc,
                                             const WshShell_Char_t* argv[], void* ctx, bool enabled) {
    if (cmd == NULL || argc == 0U || argv == NULL || ctx == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    if (!WshShell_IsAuth(shell)) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_Size_t pos = 0U;
    bool help = false;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;
            case OPT_HELP: help = true; break;
            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    if (help) {
        WshShellCmd_PrintOptionsOverview(cmd);
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    app_sound_cancel();
    if (!bsp_buzzer_set_enabled(enabled)) {
        WSH_SHELL_PRINT_ERR("Buzzer unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_INFO("Buzzer %s\n", enabled ? "enabled" : "disabled");
    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t on_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                        void* ctx) {
    return control_handler(cmd, argc, argv, ctx, true);
}

static WSH_SHELL_RET_STATE_t off_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                         void* ctx) {
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
