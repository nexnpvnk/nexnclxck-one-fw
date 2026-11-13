#include "app_cli_light.h"

#include <stddef.h>
#include <string.h>

#include "app_cli.h"
#include "app_light.h"
#include "wsh_shell.h"

enum { OPT_DEFAULT, OPT_HELP, OPT_EFFECT, OPT_PRIMARY, OPT_SECONDARY, OPT_SPEED, OPT_BRIGHTNESS, OPT_END };

static const WshShellOption_t read_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show lighting parameters")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t reset_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Restore startup lighting parameters")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t set_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Set one or more lighting parameters")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_EFFECT, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-e", "--effect",
                                   "Effect: static, breath, rainbow, fade, flicker, comet")},
    {OPT_PRIMARY, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "", "--primary", "Primary color (0xRRGGBB)")},
    {OPT_SECONDARY, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "", "--secondary", "Secondary color (0xRRGGBB)")},
    {OPT_SPEED, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-s", "--speed", "Effect speed (0..255; higher is faster)")},
    {OPT_BRIGHTNESS, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-b", "--brightness",
                                       "Lighting brightness (0..255; 0 turns lighting off)")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static bool parse_effect(const char* text, ledfx_effect_t* effect) {
    for (unsigned i = 0U; i < LEDFX_EFFECT_COUNT; ++i) {
        if (strcmp(text, ledfx_effect_name((ledfx_effect_t)i)) == 0) {
            *effect = (ledfx_effect_t)i;
            return true;
        }
    }
    return false;
}

static bool parse_color(const char* text, ledfx_rgb_t* color) {
    uint32_t rgb;
    if (strlen(text) != 8U || text[0] != '0' || (text[1] != 'x' && text[1] != 'X')
        || !app_cli_parse_u32(text, 0xFFFFFFU, true, &rgb)) {
        return false;
    }
    *color = (ledfx_rgb_t){
        .r = (uint8_t)(rgb >> 16U),
        .g = (uint8_t)(rgb >> 8U),
        .b = (uint8_t)rgb,
    };
    return true;
}

static WSH_SHELL_RET_STATE_t print_status(void) {
    app_light_status_t status;
    if (!app_light_get_status(&status)) {
        WSH_SHELL_PRINT_ERR("Lighting unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_SYS("LEDs       : %u\n"
                        "Effect     : %s\n"
                        "Primary    : 0x%02X%02X%02X\n"
                        "Secondary  : 0x%02X%02X%02X\n"
                        "Speed      : %u/255\n"
                        "Brightness : %u/255\n",
                        (unsigned)status.led_count, ledfx_effect_name(status.effect), (unsigned)status.primary.r,
                        (unsigned)status.primary.g, (unsigned)status.primary.b, (unsigned)status.secondary.r,
                        (unsigned)status.secondary.g, (unsigned)status.secondary.b, (unsigned)status.speed,
                        (unsigned)status.brightness);
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

static WSH_SHELL_RET_STATE_t reset_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc,
                                           const WshShell_Char_t* argv[], void* ctx) {
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
    if (!app_light_reset()) {
        WSH_SHELL_PRINT_ERR("Lighting unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_INFO("Startup lighting parameters restored\n");
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
    bool has_parameters = false;
    const char* values[OPT_END] = {0};
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;
            case OPT_HELP: help = true; break;
            case OPT_EFFECT:
            case OPT_PRIMARY:
            case OPT_SECONDARY:
            case OPT_SPEED:
            case OPT_BRIGHTNESS:
                if (values[opt.Option->ID] != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify %s only once\n", opt.Option->LongName);
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                values[opt.Option->ID] = argv[opt.TokenPos + 1U];
                has_parameters = true;
                break;
            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    if (help) {
        WshShellCmd_PrintOptionsOverview(cmd);
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    if (!has_parameters) {
        WSH_SHELL_PRINT_WARN("Specify lighting parameters; see light set --help\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    app_light_status_t desired;
    if (!app_light_get_status(&desired)) {
        WSH_SHELL_PRINT_ERR("Lighting unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    if (values[OPT_EFFECT] != NULL && !parse_effect(values[OPT_EFFECT], &desired.effect)) {
        WSH_SHELL_PRINT_WARN("Invalid effect: use static, breath, rainbow, fade, flicker or comet\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (values[OPT_PRIMARY] != NULL && !parse_color(values[OPT_PRIMARY], &desired.primary)) {
        WSH_SHELL_PRINT_WARN("Invalid primary color: expected 0xRRGGBB\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (values[OPT_SECONDARY] != NULL && !parse_color(values[OPT_SECONDARY], &desired.secondary)) {
        WSH_SHELL_PRINT_WARN("Invalid secondary color: expected 0xRRGGBB\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    uint32_t value;
    if (values[OPT_SPEED] != NULL) {
        if (!app_cli_parse_u32(values[OPT_SPEED], UINT8_MAX, false, &value)) {
            WSH_SHELL_PRINT_WARN("Invalid speed: expected an integer (0..255)\n");
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        desired.speed = (uint8_t)value;
    }
    if (values[OPT_BRIGHTNESS] != NULL) {
        if (!app_cli_parse_u32(values[OPT_BRIGHTNESS], UINT8_MAX, false, &value)) {
            WSH_SHELL_PRINT_WARN("Invalid brightness: expected an integer (0..255)\n");
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        desired.brightness = (uint8_t)value;
    }

    /* All values are validated. These foreground-only setters cannot fail
     * after successful status read; rendering resumes after the command. */
    if ((values[OPT_EFFECT] != NULL && !app_light_set_effect(desired.effect))
        || ((values[OPT_PRIMARY] != NULL || values[OPT_SECONDARY] != NULL)
            && !app_light_set_colors(desired.primary, desired.secondary))
        || (values[OPT_SPEED] != NULL && !app_light_set_speed(desired.speed))
        || (values[OPT_BRIGHTNESS] != NULL && !app_light_set_brightness(desired.brightness))) {
        WSH_SHELL_PRINT_ERR("Failed to apply lighting parameters\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_INFO("Lighting parameters accepted\n");
    return print_status();
}

static const WshShellCmd_t set_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "set",
    .Descr = "Set lighting: light set [--effect <name>] [--primary <0xRRGGBB>] [--secondary <0xRRGGBB>] "
             "[--speed <0..255>] [--brightness <0..255>]",
    .Options = set_opts,
    .OptNum = WSH_SHELL_ARR_LEN(set_opts),
    .Handler = set_handler,
};

static const WshShellCmd_t reset_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "reset",
    .Descr = "Restore startup lighting parameters and restart the effect",
    .Options = reset_opts,
    .OptNum = WSH_SHELL_ARR_LEN(reset_opts),
    .Handler = reset_handler,
};

static const WshShellCmd_t* const subcommands[] = {&set_cmd, &reset_cmd};

const WshShellCmd_t app_cli_light_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "light",
    .Descr = "WS2812 lighting parameters",
    .Options = read_opts,
    .OptNum = WSH_SHELL_ARR_LEN(read_opts),
    .Handler = read_handler,
    .SubCmds = subcommands,
    .SubCmdNum = WSH_SHELL_ARR_LEN(subcommands),
};
