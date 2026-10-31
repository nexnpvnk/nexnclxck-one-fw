#include <stddef.h>

#include "app_cli.h"

#include "app_cli_bme280.h"
#include "app_cli_buzzer.h"
#include "app_cli_clock.h"
#include "app_cli_hv.h"
#include "bsp_time.h"
#include "shell.h"
#include "wsh_shell.h"

static const WshShellCmd_t* app_cli_commands[] = {&app_cli_clock_cmd, &app_cli_bme280_cmd, &app_cli_buzzer_cmd,
                                                  &app_cli_hv_cmd};

static struct {
    WshShell_t* console;
    app_cli_watch_t source;
    uint32_t last_print_ms;
} watch;

static void app_cli_cancel(void) {
    watch.console = NULL;
}

static const char* watch_name(app_cli_watch_t source) {
    return source == APP_CLI_WATCH_HV ? "HV" : "BME280";
}

void app_cli_watch_start(app_cli_watch_t source, void* ctx) {
    if (watch.console != NULL && watch.source != source) {
        WSH_SHELL_PRINT_INFO("%s monitor replaced by %s\n", watch_name(watch.source), watch_name(source));
    }
    watch.console = ctx;
    watch.source = source;
    watch.last_print_ms = bsp_time_ms();
    WSH_SHELL_PRINT_SYS("%s watch; Ctrl+C to stop output\n", watch_name(source));
}

bool app_cli_init(void) {
    if (!shell_commands_attach(app_cli_commands, WSH_SHELL_ARR_LEN(app_cli_commands))) {
        return false;
    }
    shell_set_cancel_handler(app_cli_cancel);
    return true;
}

void app_cli_process(void) {
    if (watch.console == NULL) return;
    WshShell_t* console = watch.console;
    if (!WshShell_IsAuth(console) || console->Interact.Handler != NULL) {
        app_cli_cancel();
        return;
    }
    uint32_t now = bsp_time_ms();
    if ((uint32_t)(now - watch.last_print_ms) < 1000U) return;
    watch.last_print_ms = now;

    WSH_SHELL_PRINT("\r\033[2K");
    bool available = watch.source == APP_CLI_WATCH_HV ? app_cli_hv_watch_print() : app_cli_bme280_watch_print();
    if (!available) {
        WSH_SHELL_PRINT_ERR("%s monitor stopped: status unavailable\n", watch_name(watch.source));
        app_cli_cancel();
    }
    /* Background output needs to preserve the editor cursor. The shell's
     * RefreshConsoleFromInterBuff resets it to the end of the input. */
    WSH_SHELL_PRINT("%s", console->PS1);
    WshShellIO_PrintInterBuff(&console->CommandLine);
    if (console->CommandLine.Len > console->CommandLine.CursorPos) {
        WSH_SHELL_PRINT("\033[%uD", (unsigned)(console->CommandLine.Len - console->CommandLine.CursorPos));
    }
}

bool app_cli_parse_u32(const char* text, uint32_t max, bool allow_hex, uint32_t* value) {
    if (text == NULL || value == NULL || *text == '\0') {
        return false;
    }

    uint32_t base = 10U;
    uint32_t result = 0U;

    if (allow_hex && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
        base = 16U;
    }

    if (*text == '\0') {
        return false;
    }

    while (*text != '\0') {
        uint32_t digit;
        char ch = *text++;

        if (ch >= '0' && ch <= '9') {
            digit = (uint32_t)(ch - '0');
        } else if (base == 16U && ch >= 'a' && ch <= 'f') {
            digit = (uint32_t)(ch - 'a') + 10U;
        } else if (base == 16U && ch >= 'A' && ch <= 'F') {
            digit = (uint32_t)(ch - 'A') + 10U;
        } else {
            return false;
        }

        if (digit >= base || digit > max || result > (max - digit) / base) {
            return false;
        }

        result = result * base + digit;
    }

    *value = result;

    return true;
}
