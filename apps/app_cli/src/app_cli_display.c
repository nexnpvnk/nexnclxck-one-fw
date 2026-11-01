#include "app_cli_display.h"

#include <stddef.h>

#include "app_cli.h"
#include "app_display.h"
#include "wsh_shell.h"

enum { OPT_DEFAULT, OPT_HELP, OPT_BRIGHTNESS, OPT_FREQUENCY, OPT_BLANKING_TIME, OPT_END };

static const WshShellOption_t read_opts[] = {
    {OPT_DEFAULT,
     WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show display brightness, refresh target and blanking time")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t set_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Set one or more display parameters")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_BRIGHTNESS,
     WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-b", "--brightness", "Global brightness (0..100%)")},
    {OPT_FREQUENCY, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-f", "--frequency",
                                      "Full scan refresh in Hz (1..400; blanking time must fit the slot)")},
    {OPT_BLANKING_TIME, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-t", "--blanking-time",
                                          "Anode blanking in us (0..65535; must be shorter than the slot)")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static WSH_SHELL_RET_STATE_t print_status(void) {
    app_display_status_t status;
    if (!app_display_get_status(&status)) {
        WSH_SHELL_PRINT_ERR("Display unavailable; check startup profile diagnostics\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_SYS("Scan state : %s\n"
                        "Brightness : %u.%u %%\n"
                        "Refresh    : %u Hz (full scan target)\n"
                        "Blanking   : %u us\n",
                        status.running ? "running" : "stopped", (unsigned)(status.brightness_pm / 10U),
                        (unsigned)(status.brightness_pm % 10U), (unsigned)status.refresh_hz,
                        (unsigned)status.blanking_time_us);
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
    const char* brightness_text = NULL;
    const char* frequency_text = NULL;
    const char* blanking_time_text = NULL;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;
            case OPT_HELP: help = true; break;
            case OPT_BRIGHTNESS:
                if (brightness_text != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify --brightness only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                brightness_text = argv[opt.TokenPos + 1U];
                break;
            case OPT_FREQUENCY:
                if (frequency_text != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify --frequency only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                frequency_text = argv[opt.TokenPos + 1U];
                break;
            case OPT_BLANKING_TIME:
                if (blanking_time_text != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify --blanking-time only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                blanking_time_text = argv[opt.TokenPos + 1U];
                break;
            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    if (help) {
        WshShellCmd_PrintOptionsOverview(cmd);
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    if (brightness_text == NULL && frequency_text == NULL && blanking_time_text == NULL) {
        WSH_SHELL_PRINT_WARN("Specify --brightness, --frequency or --blanking-time; see display set --help\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    uint32_t brightness = 0U;
    uint32_t frequency = 0U;
    uint32_t blanking_time = 0U;
    if (brightness_text != NULL && !app_cli_parse_u32(brightness_text, 100U, false, &brightness)) {
        WSH_SHELL_PRINT_WARN("Invalid brightness: expected an integer (0..100%%)\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (frequency_text != NULL
        && (!app_cli_parse_u32(frequency_text, APP_DISPLAY_REFRESH_MAX_HZ, false, &frequency)
            || frequency < APP_DISPLAY_REFRESH_MIN_HZ)) {
        WSH_SHELL_PRINT_WARN("Invalid refresh frequency: expected an integer (%u..%u Hz)\n",
                             (unsigned)APP_DISPLAY_REFRESH_MIN_HZ, (unsigned)APP_DISPLAY_REFRESH_MAX_HZ);
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (blanking_time_text != NULL && !app_cli_parse_u32(blanking_time_text, UINT16_MAX, false, &blanking_time)) {
        WSH_SHELL_PRINT_WARN("Invalid blanking time: expected an integer (0..65535 us)\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    app_display_status_t current;
    if (!app_display_get_status(&current)) {
        WSH_SHELL_PRINT_ERR("Display unavailable; check startup profile diagnostics\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    uint16_t brightness_pm = brightness_text != NULL ? (uint16_t)(brightness * 10U) : current.brightness_pm;
    uint16_t refresh_hz = frequency_text != NULL ? (uint16_t)frequency : current.refresh_hz;
    uint16_t blanking_time_us = blanking_time_text != NULL ? (uint16_t)blanking_time : current.blanking_time_us;
    /* Validate the complete combination before changing any setting. */
    nixie_status_t result = app_display_set_parameters(brightness_pm, refresh_hz, blanking_time_us);
    if (result == NIXIE_INVALID_TIMING) {
        WSH_SHELL_PRINT_WARN("Frequency/blanking-time combination does not fit the scan slot or timer period; "
                             "settings unchanged\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (result == NIXIE_BUSY) {
        WSH_SHELL_PRINT_WARN("A display frame is pending; settings unchanged, retry command\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    if (result != NIXIE_OK) {
        WSH_SHELL_PRINT_ERR("Display parameters rejected: %s; settings unchanged\n", nixie_status_name(result));
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_INFO("Display parameters accepted\n");
    return print_status();
}

static const WshShellCmd_t set_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "set",
    .Descr = "Set display: display set [-b <0..100%>] [-f <Hz>] [-t <us>]",
    .Options = set_opts,
    .OptNum = WSH_SHELL_ARR_LEN(set_opts),
    .Handler = set_handler,
};

static const WshShellCmd_t* const subcommands[] = {&set_cmd};

const WshShellCmd_t app_cli_display_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "display",
    .Descr = "Nixie display status and parameters",
    .Options = read_opts,
    .OptNum = WSH_SHELL_ARR_LEN(read_opts),
    .Handler = read_handler,
    .SubCmds = subcommands,
    .SubCmdNum = WSH_SHELL_ARR_LEN(subcommands),
};
