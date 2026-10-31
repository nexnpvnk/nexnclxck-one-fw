#include "app_cli_bme280.h"
#include "app_bme280.h"
#include "app_cli.h"
#include "bsp_time.h"

enum { OPT_DEFAULT, OPT_HELP, OPT_WATCH, OPT_END };

static const WshShellOption_t read_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show latest temperature, pressure and humidity")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_WATCH,
     WSH_SHELL_OPT_WO_PARAM(WSH_SHELL_OPT_ACCESS_READ, "-w", "--watch", "Show cached values once per second")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const char* transport_name(ui2c_status_t status) {
    switch (status) {
        case UI2C_STATUS_NACK: return "NACK (device absent or not ready)";
        case UI2C_STATUS_BUS_BUSY: return "bus busy";
        case UI2C_STATUS_BUSY: return "bus in use";
        case UI2C_STATUS_ARBITRATION_LOST: return "arbitration lost";
        case UI2C_STATUS_TIMEOUT: return "transfer timeout";
        case UI2C_STATUS_NOT_INITIALIZED: return "bus not initialized";
        default: return "I/O failure";
    }
}

static void print_status(const app_bme280_sample_t* sample) {
    uint32_t age_ms = bsp_time_ms() - sample->sampled_at_ms;
    if (!sample->ready) {
        WSH_SHELL_PRINT_SYS("BME280: no sample yet\n");
    } else if (sample->sensor_status == 0) {
        int32_t t = sample->temperature_centi_c;
        uint32_t magnitude = t < 0 ? 0U - (uint32_t)t : (uint32_t)t;
        uint32_t p = (sample->pressure_centi_pa + 50U) / 100U; /* Bosch 64-bit: Pa * 100. */
        uint32_t h = sample->humidity_centi_percent;
        WSH_SHELL_PRINT_SYS("BME280 0x%02X | T: %s%lu.%02lu C | P: %lu.%02lu hPa | RH: %lu.%02lu %% | Age: %lu ms\n",
                            (unsigned)sample->address, t < 0 ? "-" : "", (unsigned long)(magnitude / 100U),
                            (unsigned long)(magnitude % 100U), (unsigned long)(p / 100U), (unsigned long)(p % 100U),
                            (unsigned long)(h / 100U), (unsigned long)(h % 100U), (unsigned long)age_ms);
    } else if (sample->transport_status != UI2C_STATUS_OK) {
        WSH_SHELL_PRINT_WARN("BME280 0x%02X: %s | Age: %lu ms\n", (unsigned)sample->address,
                             transport_name(sample->transport_status), (unsigned long)age_ms);
    } else {
        WSH_SHELL_PRINT_WARN("BME280 0x%02X: Bosch error %d | Age: %lu ms\n", (unsigned)sample->address,
                             (int)sample->sensor_status, (unsigned long)age_ms);
    }
}

bool app_cli_bme280_watch_print(void) {
    print_status(app_bme280_latest());
    return true;
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
    bool watch = false;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;
            case OPT_HELP: help = true; break;
            case OPT_WATCH:
                if (watch) {
                    WSH_SHELL_PRINT_WARN("Specify --watch only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                watch = true;
                break;
            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    if (help) {
        WshShellCmd_PrintOptionsOverview(cmd);
        return WSH_SHELL_RET_STATE_SUCCESS;
    }

    const app_bme280_sample_t* sample = app_bme280_latest();
    if (watch) {
        app_cli_watch_start(APP_CLI_WATCH_BME280, ctx);
    }
    print_status(sample);
    return WSH_SHELL_RET_STATE_SUCCESS;
}

const WshShellCmd_t app_cli_bme280_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_LOW_LEVEL,
    .Name = "bme280",
    .Descr = "BME280 cached values: show / watch",
    .Options = read_opts,
    .OptNum = WSH_SHELL_ARR_LEN(read_opts),
    .Handler = read_handler,
};
