/* Reference C implementation. Compiled several times with different -O levels,
   each time with FN defined to a distinct symbol name (see Makefile). */
#include <stddef.h>
#include <stdint.h>

uint32_t FN(const uint32_t *a, size_t n)
{
    uint32_t s = 0;
    for (size_t i = 0; i < n; i++)
        s += a[i];
    return s;
}
