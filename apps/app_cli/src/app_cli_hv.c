#include "app_cli_hv.h"

#include "app_cli.h"
#include "app_cli_bme280.h"
#include "app_hv.h"
#include "bsp_time.h"

#include <stddef.h>
#include <string.h>

enum { OPT_DEFAULT, OPT_HELP, OPT_WATCH, OPT_STOP, OPT_VOLTAGE, OPT_END };

static const WshShellOption_t read_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Show HV status")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_WATCH, WSH_SHELL_OPT_WO_PARAM(WSH_SHELL_OPT_ACCESS_READ, "-w", "--watch", "Monitor once per second")},
    {OPT_STOP, WSH_SHELL_OPT_WO_PARAM(WSH_SHELL_OPT_ACCESS_READ, "-s", "--stop", "Stop HV monitor")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t control_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Change HV state")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static const WshShellOption_t set_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Set target HV voltage")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_VOLTAGE, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-v", "--voltage", "100..250 V")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static WshShell_t* console;
static uint32_t last_print_ms;
static uint32_t last_samples;
static bool watching;

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

static WSH_SHELL_RET_STATE_t print_status(uint32_t rate_hz) {
    app_hv_status_t status;
    if (!app_hv_get_status(&status)) {
        WSH_SHELL_PRINT_ERR("HV unavailable\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }

    WSH_SHELL_PRINT_SYS(
        "HV %s | Set: %u V | Out: %lu.%03lu V | Filt: %lu.%03lu V | "
        "Duty: %u.%01u%% (limit %u.%01u%%) | ADC: %u | Rate: %lu Hz | "
        "Fault: %s/%u | PWM: %s | Request: %s\n",
        state_name(status.state), (unsigned)status.target_v,
        (unsigned long)(status.voltage_mv / 1000U), (unsigned long)(status.voltage_mv % 1000U),
        (unsigned long)(status.filtered_mv / 1000U), (unsigned long)(status.filtered_mv % 1000U),
        (unsigned)(status.duty_permille / 10U), (unsigned)(status.duty_permille % 10U),
        (unsigned)(status.duty_limit_permille / 10U), (unsigned)(status.duty_limit_permille % 10U),
        (unsigned)status.adc_raw, (unsigned long)rate_hz,
        fault_name(status.fault), (unsigned)status.bsp_fault,
        status.pwm_available ? "READY" : "LOCKED", status.requested_enabled ? "ON" : "OFF");
    return WSH_SHELL_RET_STATE_SUCCESS;
}

void app_cli_hv_cancel(void) { watching = false; }

void app_cli_hv_process(void) {
    if (!watching || console == NULL) return;
    if (console->CurrUser == NULL || console->Interact.Handler != NULL) {
        app_cli_hv_cancel();
        return;
    }
    uint32_t now = bsp_time_ms();
    uint32_t elapsed = now - last_print_ms;
    if (elapsed < 1000U) return;
    app_hv_status_t status;
    if (!app_hv_get_status(&status)) {
        app_cli_hv_cancel();
        return;
    }
    uint32_t rate_hz = ((status.samples - last_samples) * 1000U / elapsed);
    last_samples = status.samples;
    last_print_ms = now;

    /* Preserve partially typed shell command, as done in app_cli_bme280. */
    WSH_SHELL_PRINT("\r\033[2K");
    (void)print_status(rate_hz);
    WSH_SHELL_PRINT("%s%s", console->PS1, console->CommandLine.Buff);
    for (WshShell_Size_t i = console->CommandLine.CursorPos; i < console->CommandLine.Len; ++i) {
        WSH_SHELL_PRINT("\b");
    }
}

static WSH_SHELL_RET_STATE_t handler(const WshShellCmd_t* cmd, WshShell_Size_t argc,
                                     const WshShell_Char_t* argv[], void* ctx) {
    if (cmd == NULL || argv == NULL || ctx == NULL || ((WshShell_t*)ctx)->CurrUser == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    WshShell_Size_t pos = 0U;
    bool is_on = strcmp(cmd->Name, "on") == 0;
    bool is_off = strcmp(cmd->Name, "off") == 0;
    bool is_set = strcmp(cmd->Name, "set") == 0;
    uint32_t voltage_v = 0U;
    unsigned seen = 0U;

    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) return WSH_SHELL_RET_STATE_ERR_PARAM;
        unsigned id = opt.Option->ID;
        if (id == OPT_DEFAULT) continue;
        if (id == OPT_HELP) {
            WshShellCmd_PrintOptionsOverview(cmd);
            return WSH_SHELL_RET_STATE_SUCCESS;
        }
        if (id >= OPT_END || (seen & (1U << id))) return WSH_SHELL_RET_STATE_ERR_PARAM;
        seen |= 1U << id;
        if (id == OPT_VOLTAGE) {
            if (!is_set || (size_t)opt.TokenPos + 1U >= argc ||
                !app_cli_parse_u32(argv[opt.TokenPos + 1U], APP_HV_MAX_VOLTAGE_V, false, &voltage_v)) {
                return WSH_SHELL_RET_STATE_ERR_PARAM;
            }
        }
    }

    if (is_set) {
        if (seen != (1U << OPT_VOLTAGE) || !app_hv_set_voltage((uint16_t)voltage_v)) {
            WSH_SHELL_PRINT_WARN("Specify voltage 100..250 V\n");
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        return print_status(0U);
    }
    if (is_on || is_off) {
        if (seen != 0U) return WSH_SHELL_RET_STATE_ERR_PARAM;
        bool success = app_hv_set_enabled(is_on);
        if (!success) {
            WSH_SHELL_PRINT_WARN("HV command refused (check PWM lock, ADC, output and fault)\n");
            return WSH_SHELL_RET_STATE_ERROR;
        }
        return print_status(0U);
    }
    if (seen == (1U << OPT_STOP)) {
        app_cli_hv_cancel();
        WSH_SHELL_PRINT_SYS("HV monitor stopped\n");
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    if (seen == (1U << OPT_WATCH)) {
        app_cli_bme280_cancel();
        console = shell;
        app_hv_status_t status;
        if (!app_hv_get_status(&status)) return WSH_SHELL_RET_STATE_ERROR;
        last_samples = status.samples;
        last_print_ms = bsp_time_ms();
        watching = true;
        WSH_SHELL_PRINT_SYS("HV watch; Ctrl+C or hv -s to stop output\n");
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    if (seen != 0U) return WSH_SHELL_RET_STATE_ERR_PARAM;
    return print_status(0U);
}

static const WshShellCmd_t on_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER, .Name = "on", .Descr = "Enable HV with automatic retry",
    .Options = control_opts, .OptNum = WSH_SHELL_ARR_LEN(control_opts), .Handler = handler,
};
static const WshShellCmd_t off_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER, .Name = "off", .Descr = "Disable HV and automatic retry",
    .Options = control_opts, .OptNum = WSH_SHELL_ARR_LEN(control_opts), .Handler = handler,
};
static const WshShellCmd_t set_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER, .Name = "set", .Descr = "Set HV target voltage",
    .Options = set_opts, .OptNum = WSH_SHELL_ARR_LEN(set_opts), .Handler = handler,
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
    .Handler = handler,
    .SubCmds = subcommands,
    .SubCmdNum = WSH_SHELL_ARR_LEN(subcommands),
};
