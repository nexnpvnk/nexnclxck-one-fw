#include "bsp_storage.h"

static struct fdb_kvdb settings;
static fdb_err_t status = FDB_INIT_FAILED;

fdb_err_t bsp_storage_init(void) {
    /* Bundled FAL does not propagate device init failures. */
    if (bsp_storage_flash.ops.init() != 0) {
        return status = FDB_INIT_FAILED;
    }
    if (!settings.parent.init_ok) {
        status = fdb_kvdb_init(&settings, "settings", BSP_STORAGE_PARTITION, NULL, NULL);
    }
    return status;
}

fdb_kvdb_t bsp_storage_db(void) { return settings.parent.init_ok ? &settings : NULL; }

fdb_err_t bsp_storage_status(void) { return status; }
