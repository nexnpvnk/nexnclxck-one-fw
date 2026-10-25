#include <stddef.h>

#include "app_cli.h"

#include "app_cli_bme280.h"
#include "app_cli_buzzer.h"
#include "app_cli_clock.h"
#include "app_cli_i2c.h"
#include "app_cli_kv.h"
#include "shell.h"
#include "wsh_shell.h"

static const WshShellCmd_t* app_cli_commands[] = {
    &app_cli_clock_cmd, &app_cli_i2c_cmd, &app_cli_bme280_cmd, &app_cli_kv_cmd, &app_cli_buzzer_cmd,
};

bool app_cli_init(void) {
    if (!shell_commands_attach(app_cli_commands, WSH_SHELL_ARR_LEN(app_cli_commands))) {
        return false;
    }
    shell_set_cancel_handler(app_cli_bme280_cancel);
    return true;
}

void app_cli_process(void) { app_cli_bme280_process(); }

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
