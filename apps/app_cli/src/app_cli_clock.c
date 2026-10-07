#include "app_cli_clock.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_clock.h"
#include "wsh_shell.h"

/* clang-format off */
#define CLOCK_READ_OPT_TABLE() \
    X_CMD_ENTRY(CLOCK_READ_OPT_DEF,    WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show current date and time")) \
    X_CMD_ENTRY(CLOCK_READ_OPT_HELP,   WSH_SHELL_OPT_HELP()) \
    X_CMD_ENTRY(CLOCK_READ_OPT_END_ID, WSH_SHELL_OPT_END())
/* clang-format on */

typedef enum {

#define X_CMD_ENTRY(en, m) en,
    CLOCK_READ_OPT_TABLE()
#undef X_CMD_ENTRY
        CLOCK_READ_OPT_ENUM_SIZE
} CLOCK_READ_OPT_t;

#define X_CMD_ENTRY(en, m) {en, m},
static const WshShellOption_t clock_read_opts[] = {CLOCK_READ_OPT_TABLE()};
#undef X_CMD_ENTRY

/* clang-format off */
#define CLOCK_SET_OPT_TABLE() \
    X_CMD_ENTRY(CLOCK_SET_OPT_DEF,    WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Set date and time")) \
    X_CMD_ENTRY(CLOCK_SET_OPT_HELP,   WSH_SHELL_OPT_HELP()) \
    X_CMD_ENTRY(CLOCK_SET_OPT_DATE,   WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-d", "--date", "Date in YYYY-MM-DD format")) \
    X_CMD_ENTRY(CLOCK_SET_OPT_TIME,   WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-t", "--time", "Time in HH:MM:SS format")) \
    X_CMD_ENTRY(CLOCK_SET_OPT_END_ID, WSH_SHELL_OPT_END())
/* clang-format on */

typedef enum {

#define X_CMD_ENTRY(en, m) en,
    CLOCK_SET_OPT_TABLE()
#undef X_CMD_ENTRY
        CLOCK_SET_OPT_ENUM_SIZE
} CLOCK_SET_OPT_t;

#define X_CMD_ENTRY(en, m) {en, m},
static const WshShellOption_t clock_set_opts[] = {CLOCK_SET_OPT_TABLE()};
#undef X_CMD_ENTRY

/* clang-format off */
#define CLOCK_INVALIDATE_OPT_TABLE() \
    X_CMD_ENTRY(CLOCK_INVALIDATE_OPT_DEF,    WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_ADMIN, "Invalidate stored clock state")) \
    X_CMD_ENTRY(CLOCK_INVALIDATE_OPT_HELP,   WSH_SHELL_OPT_HELP()) \
    X_CMD_ENTRY(CLOCK_INVALIDATE_OPT_END_ID, WSH_SHELL_OPT_END())
/* clang-format on */

typedef enum {

#define X_CMD_ENTRY(en, m) en,
    CLOCK_INVALIDATE_OPT_TABLE()
#undef X_CMD_ENTRY
        CLOCK_INVALIDATE_OPT_ENUM_SIZE
} CLOCK_INVALIDATE_OPT_t;

#define X_CMD_ENTRY(en, m) {en, m},
static const WshShellOption_t clock_invalidate_opts[] = {CLOCK_INVALIDATE_OPT_TABLE()};
#undef X_CMD_ENTRY

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

    WSH_SHELL_PRINT_SYS("%s %04u-%02u-%02u %02u:%02u:%02u\r\n", weekday, (unsigned)datetime->year,
                        (unsigned)datetime->month, (unsigned)datetime->day, (unsigned)datetime->hour,
                        (unsigned)datetime->minute, (unsigned)datetime->second);
}

static WSH_SHELL_RET_STATE_t app_cli_clock_parse_read_options(const WshShellCmd_t* pcCmd, WshShell_Size_t argc,
                                                              const WshShell_Char_t* pArgv[], void* pShellCtx,
                                                              bool* help) {

    WshShell_t* shell;
    WshShell_Size_t token_pos = 0U;

    if ((pcCmd == NULL) || (pArgv == NULL) || (pShellCtx == NULL) || (help == NULL)) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    shell = (WshShell_t*)pShellCtx;
    *help = false;

    while (token_pos < argc) {
        WshShellOption_Ctx_t opt_ctx;

        opt_ctx = WshShellCmd_ParseOpt(pcCmd, argc, pArgv, shell->CurrUser->Rights, &token_pos);

        if (opt_ctx.Option == NULL) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }

        switch (opt_ctx.Option->ID) {
            case CLOCK_READ_OPT_DEF: break;

            case CLOCK_READ_OPT_HELP:
                WshShellCmd_PrintOptionsOverview(pcCmd);
                *help = true;
                break;

            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }

    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t app_cli_clock_handler(const WshShellCmd_t* pcCmd, WshShell_Size_t argc,
                                                   const WshShell_Char_t* pArgv[], void* pShellCtx) {
    app_clock_status_t status;
    app_clock_datetime_t datetime;
    bool help;

    if (app_cli_clock_parse_read_options(pcCmd, argc, pArgv, pShellCtx, &help) != WSH_SHELL_RET_STATE_SUCCESS) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    if (help) {
        return WSH_SHELL_RET_STATE_SUCCESS;
    }

    if (!app_clock_get_status(&status)) {
        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_SYS("RTC      : %s\r\n"
                        "LSE      : %s\r\n"
                        "Calendar : %s\r\n",
                        status.rtc_ready ? "ready" : "not ready",
                        status.lse_ready ? "ready" : "not ready",
                        status.datetime_valid ? "valid" : "not set");

    if (!status.datetime_valid) {
        WSH_SHELL_PRINT_WARN("Clock is not set\r\n");
        return WSH_SHELL_RET_STATE_SUCCESS;
    }

    if (!app_clock_get(&datetime)) {
        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_SYS("Date/Time: ");
    app_cli_clock_print_datetime(&datetime);

    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t app_cli_clock_set_handler(const WshShellCmd_t* pcCmd, WshShell_Size_t argc,
                                                       const WshShell_Char_t* pArgv[], void* pShellCtx) {

    WshShell_t* shell;
    WshShell_Size_t token_pos = 0U;
    app_clock_datetime_t datetime = {0};

    char date_str[11] = {0};
    char time_str[9] = {0};

    bool date_set = false;
    bool time_set = false;

    if ((pcCmd == NULL) || (pArgv == NULL) || (pShellCtx == NULL)) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    shell = (WshShell_t*)pShellCtx;

    while (token_pos < argc) {
        WshShellOption_Ctx_t opt_ctx;
        WSH_SHELL_RET_STATE_t ret;

        opt_ctx = WshShellCmd_ParseOpt(pcCmd, argc, pArgv, shell->CurrUser->Rights, &token_pos);

        if (opt_ctx.Option == NULL) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }

        switch (opt_ctx.Option->ID) {
            case CLOCK_SET_OPT_DEF: break;

            case CLOCK_SET_OPT_HELP: WshShellCmd_PrintOptionsOverview(pcCmd); return WSH_SHELL_RET_STATE_SUCCESS;

            case CLOCK_SET_OPT_DATE:
                ret = WshShellCmd_GetOptValue(&opt_ctx, argc, pArgv, sizeof(date_str) - 1U, date_str);

                if (ret != WSH_SHELL_RET_STATE_SUCCESS) {
                    return ret;
                }

                date_set = true;
                break;

            case CLOCK_SET_OPT_TIME:
                ret = WshShellCmd_GetOptValue(&opt_ctx, argc, pArgv, sizeof(time_str) - 1U, time_str);

                if (ret != WSH_SHELL_RET_STATE_SUCCESS) {
                    return ret;
                }

                time_set = true;
                break;

            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }

    if (!date_set || !time_set) {
        WSH_SHELL_PRINT_WARN("Both --date and --time are required\r\n");

        WshShellCmd_PrintOptionsOverview(pcCmd);

        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    if (!app_cli_clock_parse_date(date_str, &datetime)) {
        WSH_SHELL_PRINT_WARN("Invalid date format, expected YYYY-MM-DD\r\n");

        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    if (!app_cli_clock_parse_time(time_str, &datetime)) {
        WSH_SHELL_PRINT_WARN("Invalid time format, expected HH:MM:SS\r\n");

        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    if (!app_clock_set(&datetime)) {
        WSH_SHELL_PRINT_ERR("Failed to set clock\r\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }

    if (!app_clock_get(&datetime)) {
        WSH_SHELL_PRINT_ERR("Clock was set but readback failed\r\n");

        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_INFO("Clock updated\r\n");
    app_cli_clock_print_datetime(&datetime);

    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t app_cli_clock_invalidate_handler(const WshShellCmd_t* pcCmd, WshShell_Size_t argc,
                                                              const WshShell_Char_t* pArgv[], void* pShellCtx) {

    WshShell_t* shell;
    WshShell_Size_t token_pos = 0U;

    if ((pcCmd == NULL) || (pArgv == NULL) || (pShellCtx == NULL)) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }

    shell = (WshShell_t*)pShellCtx;

    while (token_pos < argc) {
        WshShellOption_Ctx_t opt_ctx;

        opt_ctx = WshShellCmd_ParseOpt(pcCmd, argc, pArgv, shell->CurrUser->Rights, &token_pos);

        if (opt_ctx.Option == NULL) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }

        switch (opt_ctx.Option->ID) {
            case CLOCK_INVALIDATE_OPT_DEF: break;

            case CLOCK_INVALIDATE_OPT_HELP: WshShellCmd_PrintOptionsOverview(pcCmd); return WSH_SHELL_RET_STATE_SUCCESS;

            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }

    if (!app_clock_invalidate()) {
        WSH_SHELL_PRINT_ERR("Failed to invalidate clock\r\n");

        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_INFO("Clock state invalidated\r\n");

    return WSH_SHELL_RET_STATE_SUCCESS;
}

static const WshShellCmd_t clock_set_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "set",
    .Descr = "Set RTC date and time",
    .Options = clock_set_opts,
    .OptNum = WSH_SHELL_ARR_LEN(clock_set_opts),
    .Handler = app_cli_clock_set_handler,
};

static const WshShellCmd_t clock_invalidate_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_LOW_LEVEL,
    .Name = "invalidate",
    .Descr = "Invalidate current RTC date and time",
    .Options = clock_invalidate_opts,
    .OptNum = WSH_SHELL_ARR_LEN(clock_invalidate_opts),
    .Handler = app_cli_clock_invalidate_handler,
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
    .Handler = app_cli_clock_handler,
    .SubCmds = clock_subcommands,
    .SubCmdNum = WSH_SHELL_ARR_LEN(clock_subcommands),
};
