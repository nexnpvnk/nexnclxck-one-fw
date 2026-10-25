#include "bsp_console.h"

int _write(int file, char* ptr, int len) {
    (void)file;

    if (len <= 0) {
        return 0;
    }

    static bool last_cr = false;

    for (int i = 0; i < len; ++i) {
        const char c = ptr[i];

        if (c == '\n' && !last_cr) {
            if (bsp_console_write("\r", 1U) != 1U) {
                return i;
            }
        }

        if (bsp_console_write(&c, 1U) != 1U) {
            return i;
        }

        last_cr = (c == '\r');
    }

    return len;
}
