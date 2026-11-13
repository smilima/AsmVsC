/* FNV-1a, 64-bit. FN is defined per -O level by the Makefile. */
#include <stddef.h>
#include <stdint.h>

uint64_t FN(const void *data, size_t n)
{
    const uint8_t *p = data;
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}
