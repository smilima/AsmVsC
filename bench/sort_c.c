/* Recursive quicksort (Hoare partition, middle pivot) on int32_t.
   FN is defined per -O level by the Makefile. Same algorithm as sort_asm.asm. */
#include <stddef.h>
#include <stdint.h>

static void qs(int32_t *a, long lo, long hi)
{
    if (lo >= hi)
        return;
    int32_t pivot = a[lo + (hi - lo) / 2];
    long i = lo - 1, j = hi + 1;
    for (;;) {
        do i++; while (a[i] < pivot);
        do j--; while (a[j] > pivot);
        if (i >= j)
            break;
        int32_t t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
    qs(a, lo, j);
    qs(a, j + 1, hi);
}

void FN(int32_t *a, size_t n)
{
    if (n > 1)
        qs(a, 0, (long)n - 1);
}
