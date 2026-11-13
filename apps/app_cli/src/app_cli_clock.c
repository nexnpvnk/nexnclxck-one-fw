#include "app_cli_clock.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_clock.h"
#include "wsh_shell.h"

enum { OPT_DEFAULT, OPT_HELP, OPT_DATE, OPT_TIME, OPT_END };

static const WshShellOption_t clock_read_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show RTC diagnostics, date and time")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t clock_set_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Set date and time (both required)")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_DATE, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-d", "--date", "Date YYYY-MM-DD (2000..2099)")},
    {OPT_TIME, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-t", "--time", "Time HH:MM:SS (00:00:00..23:59:59)")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t clock_invalidate_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_ADMIN, "Invalidate stored clock state")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const char* const clock_weekday_names[] = {
    "???", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun",
};

static bool app_cli_clock_is_digit(char ch) { return (ch >= '0') && (ch <= '9'); }

static uint8_t app_cli_clock_parse_u8_2(const char* str) {
    return (uint8_t)(((uint8_t)(str[0] - '0') * 10U) + (uint8_t)(str[1] - '0'));
}

static uint16_t app_cli_clock_parse_u16_4(const char* str) {
    return (uint16_t)(((uint16_t)(str[0] - '0') * 1000U) + ((uint16_t)(str[1] - '0') * 100U)
                      + ((uint16_t)(str[2] - '0') * 10U) + (uint16_t)(str[3] - '0'));
}

static bool app_cli_clock_parse_date(const char* str, app_clock_datetime_t* datetime) {
    if ((str == NULL) || (datetime == NULL)) {
        return false;
    }

    if (WSH_SHELL_STRLEN(str) != 10U) {
        return false;
    }

    if ((str[4] != '-') || (str[7] != '-')) {
        return false;
    }

    if (!app_cli_clock_is_digit(str[0]) || !app_cli_clock_is_digit(str[1]) || !app_cli_clock_is_digit(str[2])
        || !app_cli_clock_is_digit(str[3]) || !app_cli_clock_is_digit(str[5]) || !app_cli_clock_is_digit(str[6])
        || !app_cli_clock_is_digit(str[8]) || !app_cli_clock_is_digit(str[9])) {
        return false;
    }

    datetime->year = app_cli_clock_parse_u16_4(&str[0]);
    datetime->month = app_cli_clock_parse_u8_2(&str[5]);
    datetime->day = app_cli_clock_parse_u8_2(&str[8]);

    return true;
}

static bool app_cli_clock_parse_time(const char* str, app_clock_datetime_t* datetime) {
    if ((str == NULL) || (datetime == NULL)) {
        return false;
    }

    if (WSH_SHELL_STRLEN(str) != 8U) {
        return false;
    }

    if ((str[2] != ':') || (str[5] != ':')) {
        return false;
    }

    if (!app_cli_clock_is_digit(str[0]) || !app_cli_clock_is_digit(str[1]) || !app_cli_clock_is_digit(str[3])
        || !app_cli_clock_is_digit(str[4]) || !app_cli_clock_is_digit(str[6]) || !app_cli_clock_is_digit(str[7])) {
        return false;
    }

    datetime->hour = app_cli_clock_parse_u8_2(&str[0]);
    datetime->minute = app_cli_clock_parse_u8_2(&str[3]);
    datetime->second = app_cli_clock_parse_u8_2(&str[6]);

    return true;
}

static void app_cli_clock_print_datetime(const app_clock_datetime_t* datetime) {

    const char* weekday = "???";

    if ((datetime->weekday >= APP_CLOCK_WEEKDAY_MONDAY) && (datetime->weekday <= APP_CLOCK_WEEKDAY_SUNDAY)) {
        weekday = clock_weekday_names[datetime->weekday];
    }

    WSH_SHELL_PRINT_SYS("%s %04u-%02u-%02u %02u:%02u:%02u\n", weekday, (unsigned)datetime->year,
                        (unsigned)datetime->month, (unsigned)datetime->day, (unsigned)datetime->hour,
                        (unsigned)datetime->minute, (unsigned)datetime->second);
}

static WSH_SHELL_RET_STATE_t read_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                          void* ctx) {
    app_clock_status_t status;
    app_clock_datetime_t datetime;
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

    if (!app_clock_get_status(&status)) {
        WSH_SHELL_PRINT_ERR("RTC status unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_SYS("RTC      : %s\n"
                        "SOURCE   : %s\n"
                        "Calendar : %s\n",
                        status.rtc_ready ? "ready" : "not ready", status.source_ready ? "ready" : "not ready",
                        status.datetime_valid ? "valid" : "not set");

    if (!status.datetime_valid) {
        WSH_SHELL_PRINT_WARN("Clock is not set\n");
        WSH_SHELL_PRINT_SYS("Use: clock set -d YYYY-MM-DD -t HH:MM:SS (years 2000..2099)\n");
    }

    if (!app_clock_get(&datetime)) {
        WSH_SHELL_PRINT_ERR("Failed to read RTC date and time\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_SYS("Date/Time: ");
    app_cli_clock_print_datetime(&datetime);

    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t set_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                         void* ctx) {

    app_clock_datetime_t datetime = {0};
    if (cmd == NULL || argc == 0U || argv == NULL || ctx == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    if (!WshShell_IsAuth(shell)) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_Size_t pos = 0U;
    bool help = false;
    const char* date_text = NULL;
    const char* time_text = NULL;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;
            case OPT_HELP: help = true; break;
            case OPT_DATE:
                if (date_text != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify --date only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                date_text = argv[opt.TokenPos + 1U];
                break;
            case OPT_TIME:
                if (time_text != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify --time only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                time_text = argv[opt.TokenPos + 1U];
                break;
            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    if (help) {
        WshShellCmd_PrintOptionsOverview(cmd);
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    if (date_text == NULL || time_text == NULL) {
        WSH_SHELL_PRINT_WARN("Both --date and --time are required; see clock set --help\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    if (!app_cli_clock_parse_date(date_text, &datetime)) {
        WSH_SHELL_PRINT_WARN("Invalid date format: expected YYYY-MM-DD\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    if (!app_cli_clock_parse_time(time_text, &datetime)) {
        WSH_SHELL_PRINT_WARN("Invalid time format: expected HH:MM:SS\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (!app_clock_datetime_is_valid(&datetime)) {
        WSH_SHELL_PRINT_WARN("Invalid date or time: years 2000..2099, time 00:00:00..23:59:59\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    if (!app_clock_set(&datetime)) {
        WSH_SHELL_PRINT_ERR("Failed to write RTC date and time\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }

    if (!app_clock_get(&datetime)) {
        WSH_SHELL_PRINT_ERR("Clock was set but readback failed\n");

        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_INFO("Clock updated\n");
    app_cli_clock_print_datetime(&datetime);

    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t invalidate_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc,
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

    if (!app_clock_invalidate()) {
        WSH_SHELL_PRINT_ERR("Failed to invalidate clock\n");

        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_INFO("Clock state invalidated\n");

    return WSH_SHELL_RET_STATE_SUCCESS;
}

static const WshShellCmd_t clock_set_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "set",
    .Descr = "Set RTC date and time: clock set -d YYYY-MM-DD -t HH:MM:SS",
    .Options = clock_set_opts,
    .OptNum = WSH_SHELL_ARR_LEN(clock_set_opts),
    .Handler = set_handler,
};

static const WshShellCmd_t clock_invalidate_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_LOW_LEVEL,
    .Name = "invalidate",
    .Descr = "Invalidate current RTC date and time",
    .Options = clock_invalidate_opts,
    .OptNum = WSH_SHELL_ARR_LEN(clock_invalidate_opts),
    .Handler = invalidate_handler,
};

static const WshShellCmd_t* const clock_subcommands[] = {
    &clock_set_cmd,
    &clock_invalidate_cmd,
};

const WshShellCmd_t app_cli_clock_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "clock",
    .Descr = "RTC clock and calendar management",
    .Options = clock_read_opts,
    .OptNum = WSH_SHELL_ARR_LEN(clock_read_opts),
    .Handler = read_handler,
    .SubCmds = clock_subcommands,
    .SubCmdNum = WSH_SHELL_ARR_LEN(clock_subcommands),
};
