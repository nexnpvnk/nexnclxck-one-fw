#include "app_cli_i2c.h"
#include "bsp_i2c.h"

enum { OPT_DEFAULT, OPT_HELP, OPT_ADDR, OPT_REG, OPT_LEN, OPT_PROBE, OPT_END };

static const WshShellOption_t options[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Asynchronous I2C diagnostics")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_ADDR, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-a", "--addr", "7-bit address, decimal or 0xHEX")},
    {OPT_REG, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-r", "--reg", "8-bit register to read")},
    {OPT_LEN, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-n", "--len", "Read 1..32 bytes (default 1)")},
    {OPT_PROBE, WSH_SHELL_OPT_WO_PARAM(WSH_SHELL_OPT_ACCESS_WRITE, "-p", "--probe", "Address-only write")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

/* Persistent descriptors/buffers: the shell handler returns before completion. */
static struct {
    bool active;
    uint8_t reg;
    uint8_t rx[32];
    size_t count;
    ui2c_msg_t msgs[2];
} request;

static bool number(const char* text, unsigned max, unsigned* value) {
    unsigned base = 10, n = 0;
    if (text == NULL) {
        return false;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text += 2;
    }
    if (*text == '\0') {
        return false;
    }
    while (*text) {
        unsigned digit;
        char ch = *text++;
        if (ch >= '0' && ch <= '9') {
            digit = (unsigned)(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            digit = (unsigned)(ch - 'a') + 10U;
        } else if (ch >= 'A' && ch <= 'F') {
            digit = (unsigned)(ch - 'A') + 10U;
        } else {
            return false;
        }
        if (digit >= base || n > max / base || n * base + digit > max) {
            return false;
        }
        n = n * base + digit;
    }
    *value = n;
    return true;
}

static const char* status_name(ui2c_status_t status) {
    switch (status) {
        case UI2C_STATUS_OK: return "OK";
        case UI2C_STATUS_NACK: return "NACK";
        case UI2C_STATUS_BUSY: return "request pending";
        case UI2C_STATUS_BUS_BUSY: return "bus busy";
        case UI2C_STATUS_ARBITRATION_LOST: return "arbitration lost";
        case UI2C_STATUS_TIMEOUT: return "timeout";
        case UI2C_STATUS_IO_ERROR: return "I/O error";
        case UI2C_STATUS_CANCELLED: return "cancelled";
        default: return "error";
    }
}

static void completed(ui2c_t* bus, ui2c_status_t status, void* arg) {
    (void)bus;
    (void)arg;
    WSH_SHELL_PRINT_SYS("\r\nI2C: %s (%u)", status_name(status), (unsigned)status);
    if (status == UI2C_STATUS_OK) {
        for (size_t i = 0; i < request.count; i++) {
            WSH_SHELL_PRINT_SYS(" %02X", (unsigned)request.rx[i]);
        }
    }
    WSH_SHELL_PRINT_SYS("\r\n");
    request.active = false;
}

static WSH_SHELL_RET_STATE_t handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                     void* ctx) {
    if (ctx == NULL || cmd == NULL || argv == NULL) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    unsigned addr = 0, reg = 0, len = 1, seen = 0;
    WshShell_Size_t pos = 0;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.Option == NULL || opt.ParseError) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        unsigned id = opt.Option->ID;
        if (id == OPT_HELP || id == OPT_DEFAULT) {
            WshShellCmd_PrintOptionsOverview(cmd);
            return WSH_SHELL_RET_STATE_SUCCESS;
        }
        if (seen & (1U << id)) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        seen |= 1U << id;
        if (id == OPT_PROBE) {
            continue;
        }
        if (opt.TokenPos + 1U >= argc) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        unsigned* value = id == OPT_ADDR ? &addr : id == OPT_REG ? &reg : &len;
        unsigned max = id == OPT_ADDR ? 0x7F : id == OPT_REG ? 0xFF : 32;
        if (!number(argv[opt.TokenPos + 1U], max, value)) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    bool probe = (seen & (1U << OPT_PROBE)) != 0;
    if (!(seen & (1U << OPT_ADDR)) || len == 0
        || (probe ? (seen & ((1U << OPT_REG) | (1U << OPT_LEN))) != 0 : !(seen & (1U << OPT_REG)))) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    if (request.active || ui2c_busy(bsp_i2c_bus())) {
        WSH_SHELL_PRINT_WARN("I2C request pending\r\n");
        return WSH_SHELL_RET_STATE_WARNING;
    }
    request.reg = (uint8_t)reg;
    request.count = probe ? 0U : len;
    request.msgs[0] = (ui2c_msg_t){.data.tx = &request.reg, .len = probe ? 0U : 1U};
    request.msgs[1] = (ui2c_msg_t){.data.rx = request.rx, .len = len, .flags = UI2C_MSG_READ | UI2C_MSG_RESTART};
    ui2c_status_t status = ui2c_xfer(bsp_i2c_bus(), (uint16_t)addr, request.msgs, probe ? 1U : 2U, completed, NULL);
    request.active = status == UI2C_STATUS_OK;
    if (!request.active) {
        WSH_SHELL_PRINT_WARN("I2C rejected: %s (%u)\r\n", status_name(status), (unsigned)status);
        return WSH_SHELL_RET_STATE_WARNING;
    }
    WSH_SHELL_PRINT_SYS("I2C request accepted\r\n");
    return WSH_SHELL_RET_STATE_SUCCESS;
}

const WshShellCmd_t app_cli_i2c_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_LOW_LEVEL,
    .Name = "i2c",
    .Descr = "Probe / read an I2C register asynchronously",
    .Options = options,
    .OptNum = WSH_SHELL_ARR_LEN(options),
    .Handler = handler,
};
