/* strlen, naive byte loop. FN is defined per -O level by the Makefile. */
#include <stddef.h>

size_t FN(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}
