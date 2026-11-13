/* CRC-32C (Castagnoli, reflected poly 0x82F63B78, init and final xor 0xFFFFFFFF).
   Default build: classic byte-at-a-time table lookup (what portable C normally does).
   With -DHW -msse4.2: uses the compiler's SSE4.2 crc32 intrinsics instead.
   FN is defined per variant by the Makefile. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef HW
#include <nmmintrin.h>

uint32_t FN(const void *data, size_t n)
{
    const uint8_t *p = data;
    uint64_t crc = 0xFFFFFFFFu;
    while (n >= 8) {
        uint64_t v;
        memcpy(&v, p, 8);
        crc = _mm_crc32_u64(crc, v);
        p += 8;
        n -= 8;
    }
    while (n--)
        crc = _mm_crc32_u8((uint32_t)crc, *p++);
    return ~(uint32_t)crc;
}
#else
static uint32_t table[256];

__attribute__((constructor)) static void init_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0x82F63B78u & (0u - (c & 1)));
        table[i] = c;
    }
}

uint32_t FN(const void *data, size_t n)
{
    const uint8_t *p = data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++)
        crc = (crc >> 8) ^ table[(crc ^ p[i]) & 0xFF];
    return ~crc;
}
#endif
