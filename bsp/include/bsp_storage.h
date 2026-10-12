#ifndef BSP_STORAGE_H
#define BSP_STORAGE_H

#include "flashdb.h"

/* Board-owned database; use the FlashDB API directly in serialized foreground
 * context only. No calls from ISR. Writes and garbage collection are blocking. */
fdb_err_t bsp_storage_init(void);
fdb_kvdb_t bsp_storage_db(void); /* NULL if initialization failed. */
fdb_err_t bsp_storage_status(void);

#endif
