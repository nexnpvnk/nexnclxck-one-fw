#include "app_cli_buzzer.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

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
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_FREQUENCY, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-f", "--frequency", "Frequency in Hz (100..10000)")},
    {OPT_VOLUME, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-v", "--volume", "Volume (0..100%%)")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static bool parse_number(const char* text, uint32_t* number) {
    uint32_t value = 0U;

    if (text == NULL || number == NULL || *text == '\0') {
        return false;
    }

    while (*text != '\0') {
        if (*text < '0' || *text > '9') {
            return false;
        }

        uint32_t digit = (uint32_t)(*text - '0');

        if (value > (UINT32_MAX - digit) / 10U) {
            return false;
        }

        value = value * 10U + digit;
        ++text;
    }

    *number = value;
    return true;
}

static WSH_SHELL_RET_STATE_t print_status(void) {
    bsp_buzzer_status_t status;

    if (!bsp_buzzer_get_status(&status)) {
        WSH_SHELL_PRINT_ERR("Buzzer unavailable\r\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_SYS("State     : %s\r\n"
                        "Frequency : %lu Hz\r\n"
                        "Volume    : %u %%\r\n",
                        status.enabled ? "on" : "off", (unsigned long)status.frequency_hz,
                        (unsigned)status.volume_percent);

    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                     void* ctx) {
    if (cmd == NULL || argv == NULL || ctx == NULL || ((WshShell_t*)ctx)->CurrUser == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    WshShell_t* shell = ctx;
    WshShell_Size_t pos = 0U;

    bool is_set = strcmp(cmd->Name, "set") == 0;
    bool is_on = strcmp(cmd->Name, "on") == 0;
    bool is_off = strcmp(cmd->Name, "off") == 0;

    bool has_frequency = false;
    bool has_volume = false;

    uint32_t frequency = 0U;
    uint32_t volume = 0U;

    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);

        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }

        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;

            case OPT_HELP: WshShellCmd_PrintOptionsOverview(cmd); return WSH_SHELL_RET_STATE_SUCCESS;

            case OPT_FREQUENCY:
                if (!is_set || has_frequency || !parse_number(argv[opt.TokenPos + 1U], &frequency)) {
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }

                has_frequency = true;
                break;

            case OPT_VOLUME:
                if (!is_set || has_volume || !parse_number(argv[opt.TokenPos + 1U], &volume)) {
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }

                has_volume = true;
                break;

            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }

    if (is_set) {
        if (!has_frequency && !has_volume) {
            WSH_SHELL_PRINT_WARN("Specify --frequency or --volume\r\n");
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }

        if (has_frequency && (frequency < BSP_BUZZER_FREQ_MIN_HZ || frequency > BSP_BUZZER_FREQ_MAX_HZ)) {
            WSH_SHELL_PRINT_WARN("Invalid frequency\r\n");
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }

        if (has_volume && volume > BSP_BUZZER_VOLUME_MAX_PERCENT) {
            WSH_SHELL_PRINT_WARN("Invalid volume\r\n");
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }

        /* Valid manual command takes ownership from app_sound. */
        app_sound_cancel();

        if (has_frequency && !bsp_buzzer_set_frequency(frequency)) {
            WSH_SHELL_PRINT_ERR("Failed to set frequency\r\n");
            return WSH_SHELL_RET_STATE_ERROR;
        }

        if (has_volume && !bsp_buzzer_set_volume((uint8_t)volume)) {
            WSH_SHELL_PRINT_ERR("Failed to set volume\r\n");
            return WSH_SHELL_RET_STATE_ERROR;
        }

        return print_status();
    }

    if (is_on || is_off) {
        app_sound_cancel();

        if (!bsp_buzzer_set_enabled(is_on)) {
            WSH_SHELL_PRINT_ERR("Buzzer unavailable\r\n");
            return WSH_SHELL_RET_STATE_ERROR;
        }

        WSH_SHELL_PRINT_INFO("Buzzer %s\r\n", is_on ? "enabled" : "disabled");
        return WSH_SHELL_RET_STATE_SUCCESS;
    }

    return print_status();
}

static const WshShellCmd_t on_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "on",
    .Descr = "Enable buzzer",
    .Options = control_opts,
    .OptNum = WSH_SHELL_ARR_LEN(control_opts),
    .Handler = handler,
};

static const WshShellCmd_t off_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "off",
    .Descr = "Disable buzzer",
    .Options = control_opts,
    .OptNum = WSH_SHELL_ARR_LEN(control_opts),
    .Handler = handler,
};

static const WshShellCmd_t set_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "set",
    .Descr = "Set buzzer frequency",
    .Options = set_opts,
    .OptNum = WSH_SHELL_ARR_LEN(set_opts),
    .Handler = handler,
};

static const WshShellCmd_t* const subcommands[] = {
    &on_cmd,
    &off_cmd,
    &set_cmd,
};

const WshShellCmd_t app_cli_buzzer_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "buzzer",
    .Descr = "Buzzer control (TIM16 PWM)",
    .Options = read_opts,
    .OptNum = WSH_SHELL_ARR_LEN(read_opts),
    .Handler = handler,
    .SubCmds = subcommands,
    .SubCmdNum = WSH_SHELL_ARR_LEN(subcommands),
};
