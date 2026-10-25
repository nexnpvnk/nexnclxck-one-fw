#include "bsp_console.h"

#include "bsp_console.h"

int _write(int file, char* ptr, int len) {
    (void)file;

    static bool last_cr;

    if (len <= 0) {
        return 0;
    }

    int pos = 0;

    while (pos < len) {
        int start = pos;

        while (pos < len && ptr[pos] != '\n') {
            ++pos;
        }

        if (pos > start) {
            int size = pos - start;

            if (bsp_console_write(ptr + start, (size_t)size) != size) {
                return start > 0 ? start : -1;
            }

            last_cr = (ptr[pos - 1] == '\r');
        }

        if (pos < len) {
            const char* eol = last_cr ? "\n" : "\r\n";
            size_t size = last_cr ? 1U : 2U;

            if (bsp_console_write(eol, size) != (int)size) {
                return pos > 0 ? pos : -1;
            }

            last_cr = false;
            ++pos;
        }
    }

    return len;
}
