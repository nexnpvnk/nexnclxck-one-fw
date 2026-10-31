#include "app_cli_hv.h"

#include "app_cli.h"
#include "app_hv.h"
#include "bsp_time.h"

#include "bsp_hv.h"

enum { OPT_DEFAULT, OPT_HELP, OPT_WATCH, OPT_VOLTAGE, OPT_FREQUENCY, OPT_END };

static const WshShellOption_t read_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show HV status")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_WATCH, WSH_SHELL_OPT_WO_PARAM(WSH_SHELL_OPT_ACCESS_READ, "-w", "--watch",
                                      "Watch voltage, duty, PWM/ADC rates and faults once per second")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t control_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Change HV state")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t set_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Set HV voltage or PWM frequency")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_VOLTAGE, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-v", "--voltage", "100..250 V")},
    {OPT_FREQUENCY, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-f", "--frequency", "PWM frequency in Hz (20000..50000)")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static uint32_t last_print_ms;
static uint32_t last_samples;

static const char* state_name(app_hv_state_t state) {
    switch (state) {
        case APP_HV_OFF: return "OFF";
        case APP_HV_STARTING: return "STARTING";
        case APP_HV_RUNNING: return "RUNNING";
        case APP_HV_FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}

static const char* fault_name(app_hv_fault_t fault) {
    switch (fault) {
        case APP_HV_FAULT_NONE: return "NONE";
        case APP_HV_FAULT_INIT: return "INIT";
        case APP_HV_FAULT_BSP: return "BSP";
        case APP_HV_FAULT_NO_RISE: return "NO_RISE";
        case APP_HV_FAULT_NO_REGULATION: return "NO_REGULATION";
        default: return "UNKNOWN";
    }
}

static const char* bsp_fault_name(uint8_t fault) {
    switch ((bsp_hv_fault_t)fault) {
        case BSP_HV_FAULT_NONE: return "NONE";
        case BSP_HV_FAULT_INIT: return "INIT";
        case BSP_HV_FAULT_ADC: return "ADC";
        case BSP_HV_FAULT_OVERVOLTAGE: return "OVERVOLTAGE";
        case BSP_HV_FAULT_TIMEOUT: return "TIMEOUT";
        case BSP_HV_FAULT_SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

static void print_status(const app_hv_status_t* status) {
    WSH_SHELL_PRINT_SYS(
        "State            : %s\n"
        "Enable requested : %s (automatic retry %s)\n"
        "Target           : %u V\n"
        "Output           : %lu.%03lu V\n"
        "Filtered output  : %lu.%03lu V\n"
        "Duty             : %u.%01u%% (limit %u.%01u%%)\n"
        "PWM frequency    : %lu Hz\n"
        "ADC              : %u\n"
        "PWM available    : %s\n"
        "Fault            : %s / %s\n",
        state_name(status->state), status->requested_enabled ? "yes" : "no",
        status->requested_enabled ? "enabled" : "disabled", (unsigned)status->target_v,
        (unsigned long)(status->voltage_mv / 1000U), (unsigned long)(status->voltage_mv % 1000U),
        (unsigned long)(status->filtered_mv / 1000U), (unsigned long)(status->filtered_mv % 1000U),
        (unsigned)(status->duty_permille / 10U), (unsigned)(status->duty_permille % 10U),
        (unsigned)(status->duty_limit_permille / 10U), (unsigned)(status->duty_limit_permille % 10U),
        (unsigned long)status->pwm_frequency_hz,
        (unsigned)status->adc_raw, status->pwm_available ? "yes" : "no",
        fault_name(status->fault), bsp_fault_name(status->bsp_fault));
}

static void print_watch_status(const app_hv_status_t* status, const char* rate) {
    WSH_SHELL_PRINT_SYS(
        "HV %s | Out/Set: %lu.%03lu/%u V | Duty: %u.%01u%% | PWM: %lu Hz | ADC: %s | %s/%s\n",
        state_name(status->state),
        (unsigned long)(status->filtered_mv / 1000U), (unsigned long)(status->filtered_mv % 1000U),
        (unsigned)status->target_v,
        (unsigned)(status->duty_permille / 10U), (unsigned)(status->duty_permille % 10U),
        (unsigned long)status->pwm_frequency_hz, rate,
        fault_name(status->fault), bsp_fault_name(status->bsp_fault));
}

bool app_cli_hv_watch_print(void) {
    uint32_t now = bsp_time_ms();
    uint32_t elapsed = now - last_print_ms;
    app_hv_status_t status;
    if (!app_hv_get_status(&status)) return false;
    uint32_t samples = status.samples - last_samples;
    last_samples = status.samples;
    last_print_ms = now;
    /* Skip an interval that cannot be scaled safely in 32 bits. The saved
     * counters above let the next tick resume normal rate calculation. */
    if (elapsed == 0U || samples > UINT32_MAX / 1000U) {
        print_watch_status(&status, "n/a");
        return true;
    }
    uint32_t rate_hz = samples * 1000U / elapsed;
    char rate[24];
    WSH_SHELL_SNPRINTF(rate, sizeof(rate), "%lu Hz", (unsigned long)rate_hz);
    print_watch_status(&status, rate);
    return true;
}

static WSH_SHELL_RET_STATE_t read_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc,
                                          const WshShell_Char_t* argv[], void* ctx) {
    if (cmd == NULL || argc == 0U || argv == NULL || ctx == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    if (!WshShell_IsAuth(shell)) return WSH_SHELL_RET_STATE_ERR_PARAM;
    WshShell_Size_t pos = 0U;
    bool help = false;
    bool watch = false;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) return WSH_SHELL_RET_STATE_ERR_PARAM;
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
    app_hv_status_t status;
    if (!app_hv_get_status(&status)) {
        WSH_SHELL_PRINT_ERR("HV unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    if (watch) {
        last_samples = status.samples;
        last_print_ms = bsp_time_ms();
        app_cli_watch_start(APP_CLI_WATCH_HV, ctx);
        print_watch_status(&status, "n/a");
    } else {
        print_status(&status);
    }
    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t set_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc,
                                         const WshShell_Char_t* argv[], void* ctx) {
    if (cmd == NULL || argc == 0U || argv == NULL || ctx == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    if (!WshShell_IsAuth(shell)) return WSH_SHELL_RET_STATE_ERR_PARAM;
    WshShell_Size_t pos = 0U;
    bool help = false;
    const char* voltage_text = NULL;
    const char* frequency_text = NULL;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) return WSH_SHELL_RET_STATE_ERR_PARAM;
        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;
            case OPT_HELP: help = true; break;
            case OPT_VOLTAGE:
                if (voltage_text != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify --voltage only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                voltage_text = argv[opt.TokenPos + 1U];
                break;
            case OPT_FREQUENCY:
                if (frequency_text != NULL) {
                    WSH_SHELL_PRINT_WARN("Specify --frequency only once\n");
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                frequency_text = argv[opt.TokenPos + 1U];
                break;
            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    if (help) {
        WshShellCmd_PrintOptionsOverview(cmd);
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    bool has_voltage = voltage_text != NULL;
    bool has_frequency = frequency_text != NULL;
    uint32_t voltage_v = 0U;
    uint32_t frequency_hz = 0U;
    if (!has_voltage && !has_frequency) {
        WSH_SHELL_PRINT_WARN("Specify --voltage or --frequency (or both); see hv set --help\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (has_voltage && !app_cli_parse_u32(voltage_text, UINT32_MAX, false, &voltage_v)) {
        WSH_SHELL_PRINT_WARN("Invalid voltage: expected an integer (100..250 V)\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (has_voltage && (voltage_v < APP_HV_MIN_VOLTAGE_V || voltage_v > APP_HV_MAX_VOLTAGE_V)) {
        WSH_SHELL_PRINT_WARN("Voltage must be 100..250 V\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (has_frequency && !app_cli_parse_u32(frequency_text, UINT32_MAX, false, &frequency_hz)) {
        WSH_SHELL_PRINT_WARN("Invalid frequency: expected an integer (20000..50000 Hz)\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (has_frequency && (frequency_hz < BSP_HV_PWM_MIN_HZ || frequency_hz > BSP_HV_PWM_MAX_HZ)) {
        WSH_SHELL_PRINT_WARN("PWM frequency must be 20000..50000 Hz\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    /* Validate all arguments before changing either parameter. */
    if (has_frequency && !app_hv_set_frequency(frequency_hz)) {
        WSH_SHELL_PRINT_ERR("HV unavailable; PWM frequency unchanged\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    if (has_voltage && !app_hv_set_voltage((uint16_t)voltage_v)) {
        WSH_SHELL_PRINT_ERR("HV unavailable; target unchanged\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    app_hv_status_t status;
    if (!app_hv_get_status(&status)) {
        WSH_SHELL_PRINT_ERR("Parameters updated, but HV status unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_INFO("HV parameters: target %u V, PWM %lu Hz\n",
                        (unsigned)status.target_v, (unsigned long)status.pwm_frequency_hz);
    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t control_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc,
                                             const WshShell_Char_t* argv[], void* ctx, bool enabled) {
    if (cmd == NULL || argc == 0U || argv == NULL || ctx == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    if (!WshShell_IsAuth(shell)) return WSH_SHELL_RET_STATE_ERR_PARAM;
    WshShell_Size_t pos = 0U;
    bool help = false;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) return WSH_SHELL_RET_STATE_ERR_PARAM;
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
    if (!app_hv_set_enabled(enabled)) {
        WSH_SHELL_PRINT_ERR("HV unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    if (enabled) {
        WSH_SHELL_PRINT_INFO("HV enable requested; automatic retry enabled\n");
    } else {
        WSH_SHELL_PRINT_INFO("HV disabled; automatic retry disabled\n");
    }
    app_hv_status_t status;
    if (!app_hv_get_status(&status)) {
        WSH_SHELL_PRINT_ERR("Request accepted, but HV status unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_SYS("State: %s | Fault: %s/%s\n", state_name(status.state), fault_name(status.fault),
                        bsp_fault_name(status.bsp_fault));
    return WSH_SHELL_RET_STATE_SUCCESS;
}

static WSH_SHELL_RET_STATE_t on_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc,
                                        const WshShell_Char_t* argv[], void* ctx) {
    return control_handler(cmd, argc, argv, ctx, true);
}

static WSH_SHELL_RET_STATE_t off_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc,
                                         const WshShell_Char_t* argv[], void* ctx) {
    return control_handler(cmd, argc, argv, ctx, false);
}

static const WshShellCmd_t on_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER, .Name = "on", .Descr = "Enable HV with automatic retry",
    .Options = control_opts, .OptNum = WSH_SHELL_ARR_LEN(control_opts), .Handler = on_handler,
};
static const WshShellCmd_t off_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER, .Name = "off", .Descr = "Disable HV and automatic retry",
    .Options = control_opts, .OptNum = WSH_SHELL_ARR_LEN(control_opts), .Handler = off_handler,
};
static const WshShellCmd_t set_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER, .Name = "set", .Descr = "Set HV: -v <100..250 V> / -f <20000..50000 Hz>",
    .Options = set_opts, .OptNum = WSH_SHELL_ARR_LEN(set_opts), .Handler = set_handler,
};
static const WshShellCmd_t* const subcommands[] = {
    &on_cmd, &off_cmd, &set_cmd,
};

const WshShellCmd_t app_cli_hv_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "hv",
    .Descr = "High voltage: status / watch / set / on / off",
    .Options = read_opts,
    .OptNum = WSH_SHELL_ARR_LEN(read_opts),
    .Handler = read_handler,
    .SubCmds = subcommands,
    .SubCmdNum = WSH_SHELL_ARR_LEN(subcommands),
};
