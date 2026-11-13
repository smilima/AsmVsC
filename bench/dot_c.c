/* Dot product of two float arrays. FN is defined per variant by the Makefile.
   Strict IEEE ordering (no -ffast-math) forbids reordering the sum, so the compiler
   cannot vectorize this loop unless -ffast-math is given. */
#include <stddef.h>

float FN(const float *a, const float *b, size_t n)
{
    float s = 0.0f;
    for (size_t i = 0; i < n; i++)
        s += a[i] * b[i];
    return s;
}
