#ifndef BSP_FAL_CFG_H
#define BSP_FAL_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_STORAGE_ADDRESS   0x0800F800UL
#define BSP_STORAGE_SIZE      2048U
#define BSP_STORAGE_PAGE_SIZE 1024U
#define BSP_STORAGE_PARTITION "settings"

extern const struct fal_flash_dev bsp_storage_flash;
#define FAL_FLASH_DEV_TABLE {&bsp_storage_flash}
#define FAL_PART_HAS_TABLE_CFG
#define FAL_PART_TABLE {{FAL_PART_MAGIC_WORD, BSP_STORAGE_PARTITION, "settings_flash", 0, BSP_STORAGE_SIZE, 0}}

#ifdef __cplusplus
}
#endif

#endif /* BSP_FAL_CFG_H */
