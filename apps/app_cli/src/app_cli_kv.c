#include "app_cli_kv.h"
#include "bsp_storage.h"

#include <string.h>

#define CLI_VALUE_MAX 128U

enum { OPT_DEFAULT, OPT_HELP, OPT_KEY, OPT_VALUE, OPT_END };

static const WshShellOption_t read_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_READ, "Inspect persistent settings")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_KEY, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_READ, "-k", "--key",
                                "Key (1..31: letters, digits, dot, dash, underscore)")},
    {OPT_END, WSH_SHELL_OPT_END()},
};
static const WshShellOption_t write_opts[] = {
    {OPT_DEFAULT, WSH_SHELL_OPT_NO(WSH_SHELL_OPT_ACCESS_WRITE, "Modify persistent settings")},
    {OPT_HELP, WSH_SHELL_OPT_HELP()},
    {OPT_KEY, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-k", "--key", "Key")},
    {OPT_VALUE, WSH_SHELL_OPT_STR(WSH_SHELL_OPT_ACCESS_WRITE, "-v", "--value", "Text value (up to 128 bytes)")},
    {OPT_END, WSH_SHELL_OPT_END()},
};

static bool valid_key(const char* key) {
    if (!*key) {
        return false;
    }
    for (; *key; ++key) {
        if (!((*key >= 'a' && *key <= 'z') || (*key >= 'A' && *key <= 'Z') || (*key >= '0' && *key <= '9')
              || *key == '.' || *key == '-' || *key == '_')) {
            return false;
        }
    }
    return true;
}

/* Escape terminal control sequences and binary data from application-owned KVs. */
static void print_bytes(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (data[i] >= 32 && data[i] <= 126 && data[i] != '\\') {
            WSH_SHELL_PRINT("%c", data[i]);
        } else {
            WSH_SHELL_PRINT("\\x%02X", (unsigned)data[i]);
        }
    }
}

static WSH_SHELL_RET_STATE_t kv_handler(const WshShellCmd_t* cmd, WshShell_Size_t argc, const WshShell_Char_t* argv[],
                                        void* ctx) {
    if (!cmd || !argv || !ctx) {
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    WshShell_t* shell = ctx;
    char key[FDB_KV_NAME_MAX] = {0};
    char value[CLI_VALUE_MAX + 1] = {0};
    bool has_key = false, has_value = false;
    WshShell_Size_t pos = 0;
    while (pos < argc) {
        WshShellOption_Ctx_t opt = WshShellCmd_ParseOpt(cmd, argc, argv, shell->CurrUser->Rights, &pos);
        if (opt.ParseError || !opt.Option) {
            return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
        WSH_SHELL_RET_STATE_t result;
        switch (opt.Option->ID) {
            case OPT_DEFAULT: break;
            case OPT_HELP: WshShellCmd_PrintOptionsOverview(cmd); return WSH_SHELL_RET_STATE_SUCCESS;
            case OPT_KEY:
                if (has_key) {
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                if (strlen(argv[opt.TokenPos + 1]) >= sizeof(key)) {
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                result = WshShellCmd_GetOptValue(&opt, argc, argv, sizeof(key) - 1, key);
                if (result != WSH_SHELL_RET_STATE_SUCCESS) {
                    return result;
                }
                has_key = true;
                break;
            case OPT_VALUE:
                if (has_value) {
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                if (strlen(argv[opt.TokenPos + 1]) >= sizeof(value)) {
                    return WSH_SHELL_RET_STATE_ERR_PARAM;
                }
                result = WshShellCmd_GetOptValue(&opt, argc, argv, sizeof(value) - 1, value);
                if (result != WSH_SHELL_RET_STATE_SUCCESS) {
                    return result;
                }
                has_value = true;
                break;
            default: return WSH_SHELL_RET_STATE_ERR_PARAM;
        }
    }
    bool set = strcmp(cmd->Name, "set") == 0;
    bool get = strcmp(cmd->Name, "get") == 0;
    bool del = strcmp(cmd->Name, "del") == 0;
    bool list = strcmp(cmd->Name, "list") == 0;
    if ((set || get || del) != has_key || set != has_value || (has_key && !valid_key(key))) {
        WSH_SHELL_PRINT_WARN("Use kv get/del -k KEY or kv set -k KEY -v VALUE\r\n");
        return WSH_SHELL_RET_STATE_ERR_PARAM;
    }
    fdb_kvdb_t db = bsp_storage_db();
    if (!db) {
        WSH_SHELL_PRINT_ERR("Storage unavailable (FlashDB error %d)\r\n", (int)bsp_storage_status());
        return WSH_SHELL_RET_STATE_ERROR;
    }
    if (list || (!set && !get && !del)) {
        struct fdb_kv_iterator iterator;
        fdb_kv_iterator_init(db, &iterator);
        while (fdb_kv_iterate(db, &iterator)) {
            if (list) {
                print_bytes((const uint8_t*)iterator.curr_kv.name, iterator.curr_kv.name_len);
                WSH_SHELL_PRINT(" : %lu bytes\r\n", (unsigned long)iterator.curr_kv.value_len);
            }
        }
        WSH_SHELL_PRINT("%lu keys, %lu value bytes, %lu live record bytes\r\n", (unsigned long)iterator.iterated_cnt,
                        (unsigned long)iterator.iterated_value_bytes, (unsigned long)iterator.iterated_obj_bytes);
        if (!list) {
            WSH_SHELL_PRINT("FlashDB %s; 0x%08lX; %u bytes; %u-byte sectors; logical program: 4 bytes\r\n",
                            FDB_SW_VERSION, BSP_STORAGE_ADDRESS, BSP_STORAGE_SIZE, BSP_STORAGE_PAGE_SIZE);
            WSH_SHELL_PRINT("Commands: kv list | get -k KEY | set -k KEY -v VALUE | del -k KEY\r\n");
        }
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    struct fdb_kv kv;
    if (!set && !fdb_kv_get_obj(db, key, &kv)) {
        WSH_SHELL_PRINT_WARN("Key not found or record invalid\r\n");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    if (get) {
        uint8_t data[CLI_VALUE_MAX + 1];
        struct fdb_blob blob;
        size_t count = fdb_kv_get_blob(db, key, fdb_blob_make(&blob, data, sizeof(data)));
        size_t expected = kv.value_len < sizeof(data) ? kv.value_len : sizeof(data);
        if (count != expected) {
            return WSH_SHELL_RET_STATE_ERROR;
        }
        print_bytes(data, count);
        if (kv.value_len > count) {
            WSH_SHELL_PRINT(" ... [truncated; %lu bytes total]", (unsigned long)kv.value_len);
        }
        WSH_SHELL_PRINT("\r\n");
        return WSH_SHELL_RET_STATE_SUCCESS;
    }
    /* Upstream writes even identical values. Avoid console-induced wear. */
    if (set && fdb_kv_get_obj(db, key, &kv) && kv.value_len == strlen(value)) {
        uint8_t previous[CLI_VALUE_MAX];
        struct fdb_blob blob;
        size_t length = fdb_kv_get_blob(db, key, fdb_blob_make(&blob, previous, sizeof(previous)));
        if (length == kv.value_len && memcmp(previous, value, length) == 0) {
            WSH_SHELL_PRINT_INFO("Unchanged\r\n");
            return WSH_SHELL_RET_STATE_SUCCESS;
        }
    }
    fdb_err_t error = set ? fdb_kv_set(db, key, value) : fdb_kv_del(db, key);
    if (error != FDB_NO_ERR) {
        WSH_SHELL_PRINT_ERR("FlashDB error %d%s\r\n", (int)error, error == FDB_SAVED_FULL ? " (storage full)" : "");
        return WSH_SHELL_RET_STATE_ERROR;
    }
    WSH_SHELL_PRINT_INFO("%s\r\n", set ? "Saved" : "Deleted");
    return WSH_SHELL_RET_STATE_SUCCESS;
}

#define KV_COMMAND(symbol, name, description, opts)                                                                    \
    static const WshShellCmd_t symbol = {                                                                              \
        .Groups = WSH_SHELL_CMD_GROUP_USER,                                                                            \
        .Name = name,                                                                                                  \
        .Descr = description,                                                                                          \
        .Options = opts,                                                                                               \
        .OptNum = WSH_SHELL_ARR_LEN(opts),                                                                             \
        .Handler = kv_handler,                                                                                         \
    }
KV_COMMAND(get_cmd, "get", "Read persistent value", read_opts);
KV_COMMAND(set_cmd, "set", "Save text value", write_opts);
KV_COMMAND(del_cmd, "del", "Delete persistent value", write_opts);
KV_COMMAND(list_cmd, "list", "List keys and lengths", read_opts);
KV_COMMAND(info_cmd, "info", "Storage geometry and live data", read_opts);
static const WshShellCmd_t* const subcommands[] = {&get_cmd, &set_cmd, &del_cmd, &list_cmd, &info_cmd};

const WshShellCmd_t app_cli_kv_cmd = {
    .Groups = WSH_SHELL_CMD_GROUP_USER,
    .Name = "kv",
    .Descr = "Persistent settings (FlashDB)",
    .Options = read_opts,
    .OptNum = WSH_SHELL_ARR_LEN(read_opts),
    .Handler = kv_handler,
    .SubCmds = subcommands,
    .SubCmdNum = WSH_SHELL_ARR_LEN(subcommands),
};
