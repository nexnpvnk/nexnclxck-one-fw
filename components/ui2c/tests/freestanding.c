/* Tiny test-only runtime for Clang/LLD on Windows without the Windows SDK. */
#include <stddef.h>

void* memset(void* dst, int value, size_t count) {
    unsigned char* p = dst;
    while (count--) {
        *p++ = (unsigned char)value;
    }
    return dst;
}

void* memcpy(void* dst, const void* src, size_t count) {
    unsigned char* p = dst;
    const unsigned char* q = src;
    while (count--) {
        *p++ = *q++;
    }
    return dst;
}
