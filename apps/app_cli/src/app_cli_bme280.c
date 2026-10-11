#include "app_cli_bme280.h"
#include "app_bme280.h"
#include "bsp_time.h"

enum { OPT_DEFAULT, OPT_HELP, OPT_WATCH, OPT_STOP, OPT_END };

static const WshShellOption_t options[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show latest temperature, pressure and humidity")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_WATCH,
     WSH_SHELL_OPT_WO_PARAM(WSH_SHELL_OPT_ACCESS_READ, "-w", "--watch", "Show cached values once per second")},
    {OPT_STOP, WSH_SHELL_OPT_WO_PARAM(WSH_SHELL_OPT_ACCESS_READ, "-s", "--stop", "Stop console output")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static bool requested;
static WshShell_t* console;
static bool watching;
static uint32_t last_start;

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

void app_cli_bme280_cancel(void) {
    watching = false;
    requested = false;
}

void app_cli_bme280_process(void) {
    if (console == NULL) {
        return;
    }
    if (console->CurrUser == NULL || console->Interact.Handler != NULL) {
        app_cli_bme280_cancel();
        return;
    }
    uint32_t now = bsp_time_ms();
    if (!requested && !(watching && (uint32_t)(now - last_start) >= 1000U)) {
        return;
    }
    requested = false;
    last_start = now;
    const app_bme280_sample_t* sample = app_bme280_latest();
    /* Preserve a partially typed command while watch output is printed. */
    WSH_SHELL_PRINT("\r\033[2K");
    if (!sample->ready) {
        WSH_SHELL_PRINT_SYS("BME280: no sample yet\r\n");
    } else if (sample->sensor_status == 0) {
        int32_t t = sample->temperature_centi_c;
        uint32_t magnitude = (uint32_t)(t < 0 ? -t : t);
        uint32_t p = (sample->pressure_centi_pa + 50U) / 100U; /* Bosch 64-bit: Pa * 100. */
        uint32_t h = sample->humidity_centi_percent;
        WSH_SHELL_PRINT_SYS("BME280 0x%02X | T: %s%lu.%02lu C | P: %lu.%02lu hPa | RH: %lu.%02lu %%\r\n",
                            (unsigned)sample->address, t < 0 ? "-" : "", (unsigned long)(magnitude / 100U),
                            (unsigned long)(magnitude % 100U), (unsigned long)(p / 100U), (unsigned long)(p % 100U),
                            (unsigned long)(h / 100U), (unsigned long)(h % 100U));
    } else if (sample->transport_status != UI2C_STATUS_OK) {
        WSH_SHELL_PRINT_WARN("BME280 0x%02X: %s\r\n", (unsigned)sample->address,
                             transport_name(sample->transport_status));
    } else {
        WSH_SHELL_PRINT_WARN("BME280 0x%02X: Bosch error %d\r\n", (unsigned)sample->address,
                             (int)sample->sensor_status);
    }
    WSH_SHELL_PRINT("%s%s", console->PS1, console->CommandLine.Buff);
    for (WshShell_Size_t i = console->CommandLine.CursorPos; i < console->CommandLine.Len; ++i) {
        WSH_SHELL_PRINT("\b");
    }
}

static WSH_SHELL_RET_STATE_t handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                     void* ctx) {
    if (cmd == NULL || argv == NULL || ctx == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    WshShell_Size_t pos = 0;
    unsigned seen = 0;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        unsigned id = opt.Option->ID;
        if (id == OPT_DEFAULT) {
            continue;
        }
        if (id == OPT_HELP) {
            WshShellCmd_PrintOptionsOverview(cmd);
            return WSH_SHELL_RET_STATE_SUCCESS;
        }
        if (id >= OPT_END || (seen & (1U << id))) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        seen |= 1U << id;
    }
    if (seen & (1U << OPT_STOP)) {
        if (seen != (1U << OPT_STOP)) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        app_cli_bme280_cancel();
        WSH_SHELL_PRINT_SYS("BME280 output stopped\r\n");
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    console = shell;
    last_start = bsp_time_ms();
    if (seen & (1U << OPT_WATCH)) {
        watching = true;
        WSH_SHELL_PRINT_SYS("BME280 watch; Ctrl+C or bme280 --stop to stop output\r\n");
    }
    requested = true;
    return WSH_SHELL_RET_STATE_SUCCESS;
}

const WshShellCmd_t app_cli_bme280_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_LOW_LEVEL,
    .Name = "bme280",
    .Descr = "BME280 cached values: show / watch / stop",
    .Options = options,
    .OptNum = WSH_SHELL_ARR_LEN(options),
    .Handler = handler,
};
