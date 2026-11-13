/* Sieve of Eratosthenes. Compiled several times with different -O levels, each time
   with FN defined to a distinct symbol name (see Makefile).
   buf must hold limit+1 bytes and limit must be >= 2. Returns the number of primes <= limit. */
#include <stdint.h>
#include <string.h>

uint32_t FN(uint8_t *buf, uint32_t limit)
{
    memset(buf, 1, (size_t)limit + 1); /* 1 = "still looks prime" */
    buf[0] = buf[1] = 0;

    for (uint64_t p = 2; p * p <= limit; p++) {
        if (buf[p]) {
            for (uint64_t j = p * p; j <= limit; j += p)
                buf[j] = 0;
        }
    }

    uint32_t count = 0;
    for (uint32_t i = 2; i <= limit; i++)
        count += buf[i];
    return count;
}
