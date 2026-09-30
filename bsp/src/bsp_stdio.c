#include "bsp_console.h"

int _write(int file, char* ptr, int len) {
    (void)file;

    return bsp_console_write(ptr, (size_t)len);
}
