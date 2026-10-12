#ifndef BSP_FDB_CFG_H
#define BSP_FDB_CFG_H

#define FDB_USING_KVDB
#define FDB_USING_FAL_MODE
/* F051 programs halfwords. The port splits each logical word into two writes. */
#define FDB_WRITE_GRAN              32
#define FDB_KV_NAME_MAX             32
#define FDB_KV_CACHE_TABLE_SIZE     0
#define FDB_SECTOR_CACHE_TABLE_SIZE 0

#endif
