#include "app_cli_bme280.h"
#include "app_bme280.h"
#include "app_cli.h"

enum { OPT_DEFAULT, OPT_HELP, OPT_WATCH, OPT_END };

static const WshShellOption_t read_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show latest temperature, pressure and humidity")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_WATCH, WSH_SHELL_OPT_WO_PARAM(WSH_SHELL_OPT_ACCESS_READ, "-w", "--watch", "Show readings once per second")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static void print_readings(void) {
    app_bme280_readings_t readings;
    if (!app_bme280_get(&readings)) {
        WSH_SHELL_PRINT_WARN("BME280: readings unavailable\n");
        return;
    }

    int32_t t = readings.temperature_centi_c;
    uint32_t magnitude = t < 0 ? 0U - (uint32_t)t : (uint32_t)t;
    /* Round Pa * 100 to integer mmHg, matching the display. */
    uint32_t pressure_mmhg = (readings.pressure_centi_pa * 10U + 66661U) / 133322U;
    uint32_t h = readings.humidity_centi_percent;
    WSH_SHELL_PRINT_SYS(
        "BME280 | T: %s%lu.%02lu C | P: %lu mmHg | RH: %lu.%02lu %%\n",
        t < 0 ? "-" : "",
        (unsigned long)(magnitude / 100U),
        (unsigned long)(magnitude % 100U),
        (unsigned long)pressure_mmhg,
        (unsigned long)(h / 100U),
        (unsigned long)(h % 100U)
    );
}

bool app_cli_bme280_watch_print(void) {
    print_readings();
    return true;
}

static WSH_SHELL_RET_STATE_t read_handler(
    const WshShellCmd_t* cmd,
    WshShell_Size_t argc,
    const WshShell_Char_t* argv[],
    void* ctx
) {
    if (cmd == NULL || argc == 0U || argv == NULL || ctx == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    if (!WshShell_IsAuth(shell)) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_Size_t pos = 0U;
    bool help = false;
    bool watch = false;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        switch (opt.Option->ID) {
            case OPT_DEFAULT:
                break;
            case OPT_HELP:
                help = true;
                break;
            case OPT_WATCH:
                if (watch) {
                    WSH_SHELL_PRINT_WARN("Specify --watch only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                watch = true;
                break;
            default:
                return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    if (help) {
        WshShellCmd_PrintOptionsOverview(cmd);
        return WSH_SHELL_RET_STATE_SUCCESS;
    }

    if (watch) {
        app_cli_watch_start(APP_CLI_WATCH_BME280, ctx);
    }
    print_readings();
    return WSH_SHELL_RET_STATE_SUCCESS;
}

const WshShellCmd_t app_cli_bme280_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_LOW_LEVEL,
    .Name = "bme280",
    .Descr = "BME280 readings: show / watch",
    .Options = read_opts,
    .OptNum = WSH_SHELL_ARR_LEN(read_opts),
    .Handler = read_handler,
};
