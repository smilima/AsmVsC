/* Mandelbrot escape-time counts over [-2,1] x [-1.5,1.5]. Returns the total number of
   iterations performed over all pixels, which makes results exactly comparable.
   FN is defined per -O level by the Makefile. Built without FMA so rounding is identical
   to the assembly versions. */
#include <stdint.h>

uint64_t FN(int w, int h, int max_iter)
{
    const double dx = 3.0 / w, dy = 3.0 / h;
    uint64_t total = 0;

    for (int y = 0; y < h; y++) {
        const double ci = -1.5 + y * dy;
        for (int x = 0; x < w; x++) {
            const double cr = -2.0 + x * dx;
            double zr = 0.0, zi = 0.0;
            int it = 0;
            while (it < max_iter && zr * zr + zi * zi <= 4.0) {
                double t = 2.0 * zr * zi + ci;
                zr = zr * zr - zi * zi + cr;
                zi = t;
                it++;
            }
            total += (uint64_t)it;
        }
    }
    return total;
}
