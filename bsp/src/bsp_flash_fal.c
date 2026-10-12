#include "fal.h"
#include "stm32f0xx_hal.h"

#include <stdbool.h>
#include <string.h>

/* The linker excludes this region from the firmware image. Check its symbols
 * at runtime as well, so a regenerated linker script cannot silently corrupt code. */
extern const uint8_t __settings_start__;
extern const uint8_t __settings_end__;

_Static_assert(FLASH_PAGE_SIZE == BSP_STORAGE_PAGE_SIZE, "Unexpected flash page size");

static int flash_init(void) {
    return ((uintptr_t)&__settings_start__ == BSP_STORAGE_ADDRESS
            && (uintptr_t)&__settings_end__ == BSP_STORAGE_ADDRESS + BSP_STORAGE_SIZE)
               ? 0
               : -1;
}

static bool valid_range(long offset, size_t size) {
    return flash_init() == 0 && offset >= 0 && (size_t)offset <= BSP_STORAGE_SIZE
           && size <= BSP_STORAGE_SIZE - (size_t)offset;
}

static int flash_read(long offset, uint8_t* buf, size_t size) {
    if (!valid_range(offset, size) || (size && !buf)) {
        return -1;
    }
    if (size) {
        memcpy(buf, (const void*)(uintptr_t)(BSP_STORAGE_ADDRESS + offset), size);
    }
    return (int)size;
}

static int flash_write(long offset, const uint8_t* buf, size_t size) {
    if (!valid_range(offset, size) || (offset & 3) || (size & 3) || (size && !buf)) {
        return -1;
    }
    if (!size) {
        return 0;
    }
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -1;
    }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    int result = (int)size;
    for (size_t i = 0; i < size; i += 2) {
        uint16_t value;
        memcpy(&value, buf + i, sizeof(value));
        uint32_t address = BSP_STORAGE_ADDRESS + (uint32_t)offset + (uint32_t)i;
        uint16_t old = *(volatile const uint16_t*)(uintptr_t)address;
        /* Never reprogram non-erased halfwords. All-FF padding needs no write. */
        if (old == value) {
            continue;
        }
        if (old != UINT16_MAX || HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, value) != HAL_OK
            || *(volatile const uint16_t*)(uintptr_t)address != value) {
            result = -1;
            break;
        }
    }
    if (HAL_FLASH_Lock() != HAL_OK) {
        result = -1;
    }
    return result;
}

static int flash_erase(long offset, size_t size) {
    if (!valid_range(offset, size) || (offset % BSP_STORAGE_PAGE_SIZE) || (size % BSP_STORAGE_PAGE_SIZE)) {
        return -1;
    }
    if (!size) {
        return 0;
    }
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -1;
    }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .PageAddress = BSP_STORAGE_ADDRESS + (uint32_t)offset,
        .NbPages = size / BSP_STORAGE_PAGE_SIZE,
    };
    uint32_t page_error;
    int result = HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK ? (int)size : -1;
    if (HAL_FLASH_Lock() != HAL_OK) {
        result = -1;
    }
    if (result >= 0) {
        for (size_t i = 0; i < size; i += 2) {
            if (*(volatile const uint16_t*)(uintptr_t)(erase.PageAddress + i) != UINT16_MAX) {
                return -1;
            }
        }
    }
    return result;
}

const struct fal_flash_dev bsp_storage_flash = {
    .name = "settings_flash",
    .addr = BSP_STORAGE_ADDRESS,
    .len = BSP_STORAGE_SIZE,
    .blk_size = BSP_STORAGE_PAGE_SIZE,
    .ops = {flash_init, flash_read, flash_write, flash_erase},
    .write_gran = 32,
};
