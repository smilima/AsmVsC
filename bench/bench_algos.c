/* Benchmarks for the "other algorithms": FNV-1a, CRC-32C, SHA-256, strlen, dot product,
   Mandelbrot and quicksort. Every implementation is checked for correctness before it is
   timed; rows needing a CPU feature this machine lacks are reported as skipped. */
#define _POSIX_C_SOURCE 200809L
#include <cpuid.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"

/* ------------------------------------------------------------------ implementations */
uint64_t fnv_c_O0(const void *, size_t);
uint64_t fnv_c_O2(const void *, size_t);
uint64_t fnv_c_O3(const void *, size_t);
uint64_t fnv_asm(const void *, size_t);

uint32_t crc32_c_O0(const void *, size_t);
uint32_t crc32_c_O2(const void *, size_t);
uint32_t crc32_c_O3(const void *, size_t);
uint32_t crc32_c_hw(const void *, size_t);
uint32_t crc32_asm(const void *, size_t);

void sha256_c_O0(uint32_t *, const uint8_t *, size_t);
void sha256_c_O2(uint32_t *, const uint8_t *, size_t);
void sha256_c_O3(uint32_t *, const uint8_t *, size_t);
void sha256_asm(uint32_t *, const uint8_t *, size_t);

size_t strlen_c_O0(const char *);
size_t strlen_c_O2(const char *);
size_t strlen_c_O3(const char *);
size_t strlen_asm(const char *);

float dot_c_O0(const float *, const float *, size_t);
float dot_c_O2(const float *, const float *, size_t);
float dot_c_O3(const float *, const float *, size_t);
float dot_c_O3fast(const float *, const float *, size_t);
float dot_asm(const float *, const float *, size_t);

uint64_t mandel_c_O0(int, int, int);
uint64_t mandel_c_O2(int, int, int);
uint64_t mandel_c_O3(int, int, int);
uint64_t mandel_asm_sse2(int, int, int);
uint64_t mandel_asm_avx2(int, int, int);

void sort_c_O0(int32_t *, size_t);
void sort_c_O2(int32_t *, size_t);
void sort_c_O3(int32_t *, size_t);
void sort_asm(int32_t *, size_t);

/* ------------------------------------------------------------------ generic harness */
/* A loop function calls the implementation `reps` times (directly, with its real
   signature) and returns the sum of the results. */
typedef uint64_t (*loop_fn)(const void *fn, const void *data, size_t n, int reps);
typedef int (*verify_fn)(const void *fn, const void *data, size_t n);

struct impl {
    const char *name;
    const void *fn;
    const char *needs;      /* CPU feature required, or NULL */
    int (*avail)(void);     /* returns nonzero if the CPU has it */
};

static volatile uint64_t algo_sink; /* keeps the calls from being optimized away */

static int has_sse42(void) { return __builtin_cpu_supports("sse4.2"); }
static int has_avx2(void) { return __builtin_cpu_supports("avx2"); }
static int has_avx2_fma(void) { return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma"); }
static int has_sha(void)
{
    unsigned a, b, c, d;
    return __get_cpuid_count(7, 0, &a, &b, &c, &d) && (b & (1u << 29));
}

static uint64_t rng_state = 88172645463325252ull;
static uint64_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

/* Best (minimum) ns for one call; each trial runs enough calls to take about 50 ms. */
static double best_ns(loop_fn loop, const void *fn, const void *data, size_t n)
{
    enum { TRIALS = 5 };
    const double trial_ns = 50e6;

    algo_sink = loop(fn, data, n, 1); /* warm up */
    double t0 = now_ns();
    algo_sink = loop(fn, data, n, 1);
    double one = now_ns() - t0;
    if (one < 1.0)
        one = 1.0;
    int reps = (int)(trial_ns / one) + 1;

    double best = 1e30;
    for (int t = 0; t < TRIALS; t++) {
        double start = now_ns();
        algo_sink = loop(fn, data, n, reps);
        double per_call = (now_ns() - start) / reps;
        if (per_call < best)
            best = per_call;
    }
    return best;
}

/* Verifies and times every implementation. The first row is the reference answer unless a
   custom `verify` is given. Rate = units_per_call / ns * scale, shown under unit_label.
   Row `base` (C -O2) is the 1.00x baseline for the last column. */
static int run_table(const char *title, const struct impl *impls, int count, loop_fn loop,
                     verify_fn verify, const void *data, size_t n, double units, double scale,
                     const char *unit_label, int base)
{
    emit("\n%s\n", title);
    emit("  %-20s %12s %12s %9s\n", "impl", "ns/call", unit_label, "vs C -O2");

    uint64_t ref = loop(impls[0].fn, data, n, 1);
    double base_ns = 0;
    for (int i = 0; i < count; i++) {
        if (impls[i].avail && !impls[i].avail()) {
            emit("  %-20s skipped (CPU lacks %s)\n", impls[i].name, impls[i].needs);
            continue;
        }
        int ok = verify ? verify(impls[i].fn, data, n) : loop(impls[i].fn, data, n, 1) == ref;
        if (!ok) {
            emit("  %-20s WRONG RESULT\n", impls[i].name);
            return 1;
        }
        double ns = best_ns(loop, impls[i].fn, data, n);
        if (i == base)
            base_ns = ns;
        emit("  %-20s %12.1f %12.2f", impls[i].name, ns, units / ns * scale);
        if (base_ns > 0)
            emit(" %8.2fx", base_ns / ns);
        emit("\n");
        fflush(stdout);
    }
    return 0;
}

static void *alloc_bytes(size_t n)
{
    void *p = aligned_alloc(64, (n + 63) & ~(size_t)63);
    if (!p)
        exit(1);
    return p;
}

static void fill_random(uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++)
        p[i] = (uint8_t)rng();
}

/* ------------------------------------------------------------------ FNV-1a */
typedef uint64_t (*hash64_fn)(const void *, size_t);

static uint64_t loop_hash64(const void *fn, const void *data, size_t n, int reps)
{
    hash64_fn f = (hash64_fn)fn;
    uint64_t acc = 0;
    for (int r = 0; r < reps; r++)
        acc += f(data, n);
    return acc;
}

int run_fnv_test(void)
{
    const struct impl impls[] = {
        {"C   -O0", (const void *)fnv_c_O0, NULL, NULL},
        {"C   -O2", (const void *)fnv_c_O2, NULL, NULL},
        {"C   -O3", (const void *)fnv_c_O3, NULL, NULL},
        {"asm", (const void *)fnv_asm, NULL, NULL},
    };
    if (fnv_c_O0("a", 1) != 0xaf63dc4c8601ec8cull) { /* published FNV-1a test vector */
        emit("reference FNV-1a failed its known-answer test\n");
        return 1;
    }

    static const size_t sizes[] = {1u << 14, 1u << 20, 1u << 24};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        uint8_t *buf = alloc_bytes(sizes[s]);
        fill_random(buf, sizes[s]);
        char title[64];
        snprintf(title, sizeof title, "%zu bytes (%.0f KiB)", sizes[s], sizes[s] / 1024.0);
        int rc = run_table(title, impls, 4, loop_hash64, NULL, buf, sizes[s], (double)sizes[s], 1.0,
                           "GB/s", 1);
        free(buf);
        if (rc)
            return rc;
    }
    return 0;
}

/* ------------------------------------------------------------------ CRC-32C */
typedef uint32_t (*hash32_fn)(const void *, size_t);

static uint64_t loop_hash32(const void *fn, const void *data, size_t n, int reps)
{
    hash32_fn f = (hash32_fn)fn;
    uint64_t acc = 0;
    for (int r = 0; r < reps; r++)
        acc += f(data, n);
    return acc;
}

int run_crc32_test(void)
{
    const struct impl impls[] = {
        {"C   -O0 table", (const void *)crc32_c_O0, NULL, NULL},
        {"C   -O2 table", (const void *)crc32_c_O2, NULL, NULL},
        {"C   -O3 table", (const void *)crc32_c_O3, NULL, NULL},
        {"C   -O2 intrinsics", (const void *)crc32_c_hw, "SSE4.2", has_sse42},
        {"asm crc32 instr", (const void *)crc32_asm, "SSE4.2", has_sse42},
    };
    if (crc32_c_O0("123456789", 9) != 0xE3069283u) { /* published CRC-32C check value */
        emit("reference CRC-32C failed its known-answer test\n");
        return 1;
    }

    static const size_t sizes[] = {1u << 14, 1u << 20, 1u << 24};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        uint8_t *buf = alloc_bytes(sizes[s]);
        fill_random(buf, sizes[s]);
        char title[64];
        snprintf(title, sizeof title, "%zu bytes (%.0f KiB)", sizes[s], sizes[s] / 1024.0);
        int rc = run_table(title, impls, 5, loop_hash32, NULL, buf, sizes[s], (double)sizes[s], 1.0,
                           "GB/s", 1);
        free(buf);
        if (rc)
            return rc;
    }
    return 0;
}

/* ------------------------------------------------------------------ SHA-256 */
typedef void (*sha_fn)(uint32_t *, const uint8_t *, size_t);

static const uint32_t sha_iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

static uint64_t loop_sha(const void *fn, const void *data, size_t n, int reps)
{
    sha_fn f = (sha_fn)fn;
    uint64_t acc = 0;
    for (int r = 0; r < reps; r++) {
        uint32_t st[8];
        memcpy(st, sha_iv, sizeof st);
        f(st, data, n);
        uint64_t h = 0;
        for (int i = 0; i < 8; i++)
            h = h * 1000003u + st[i];
        acc += h;
    }
    return acc;
}

/* SHA-256("abc") through the implementation, compared with the published digest. */
static int sha_known_answer(sha_fn f)
{
    uint8_t block[64] = {'a', 'b', 'c', 0x80};
    block[63] = 24; /* message length in bits */
    uint32_t st[8];
    memcpy(st, sha_iv, sizeof st);
    f(st, block, 1);
    static const uint32_t expect[8] = {0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223,
                                       0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad};
    return memcmp(st, expect, sizeof st) == 0;
}

int run_sha256_test(void)
{
    const struct impl impls[] = {
        {"C   -O0", (const void *)sha256_c_O0, NULL, NULL},
        {"C   -O2", (const void *)sha256_c_O2, NULL, NULL},
        {"C   -O3", (const void *)sha256_c_O3, NULL, NULL},
        {"asm SHA-NI", (const void *)sha256_asm, "SHA-NI", has_sha},
    };
    if (!sha_known_answer(sha256_c_O0) || (has_sha() && !sha_known_answer(sha256_asm))) {
        emit("SHA-256 failed its known-answer test (digest of \"abc\")\n");
        return 1;
    }

    const size_t bytes = 1u << 20;
    uint8_t *buf = alloc_bytes(bytes);
    fill_random(buf, bytes);
    int rc = run_table("1048576 bytes (1 MiB, 16384 blocks)", impls, 4, loop_sha, NULL, buf, bytes / 64,
                       (double)bytes, 1.0, "GB/s", 1);
    free(buf);
    return rc;
}

/* ------------------------------------------------------------------ strlen */
typedef size_t (*strlen_fn)(const char *);

static uint64_t loop_strlen(const void *fn, const void *data, size_t n, int reps)
{
    (void)n;
    strlen_fn f = (strlen_fn)fn;
    uint64_t acc = 0;
    for (int r = 0; r < reps; r++)
        acc += f(data);
    return acc;
}

/* Every start alignment 0..63 and lengths 0..200, including a NUL at every position
   relative to the 16- and 64-byte boundaries the asm loops care about. */
static int strlen_alignment_check(strlen_fn f)
{
    char *buf = alloc_bytes(512);
    for (size_t off = 0; off < 64; off++) {
        for (size_t len = 0; len <= 200; len++) {
            memset(buf, 'x', 512);
            buf[off + len] = '\0';
            if (f(buf + off) != len) {
                free(buf);
                return 0;
            }
        }
    }
    free(buf);
    return 1;
}

static size_t libc_strlen(const char *s) { return strlen(s); }

int run_strlen_test(void)
{
    const struct impl impls[] = {
        {"C   -O0", (const void *)strlen_c_O0, NULL, NULL},
        {"C   -O2", (const void *)strlen_c_O2, NULL, NULL},
        {"C   -O3", (const void *)strlen_c_O3, NULL, NULL},
        {"asm SSE2", (const void *)strlen_asm, NULL, NULL},
        {"libc strlen", (const void *)libc_strlen, NULL, NULL},
    };
    if (!strlen_alignment_check(strlen_c_O0) || !strlen_alignment_check(strlen_asm)) {
        emit("strlen failed the alignment/length sweep\n");
        return 1;
    }

    static const size_t lens[] = {31, 4095, 1u << 20};
    for (size_t s = 0; s < sizeof lens / sizeof lens[0]; s++) {
        char *buf = alloc_bytes(lens[s] + 1);
        memset(buf, 'x', lens[s]);
        buf[lens[s]] = '\0';
        char title[64];
        snprintf(title, sizeof title, "string of %zu characters", lens[s]);
        int rc = run_table(title, impls, 5, loop_strlen, NULL, buf, lens[s], (double)lens[s], 1.0,
                           "GB/s", 1);
        free(buf);
        if (rc)
            return rc;
    }
    return 0;
}

/* ------------------------------------------------------------------ dot product */
typedef float (*dot_fn)(const float *, const float *, size_t);

struct dot_ctx {
    const float *a, *b;
    double ref, tol; /* double-precision result and allowed error */
};

static uint64_t loop_dot(const void *fn, const void *data, size_t n, int reps)
{
    dot_fn f = (dot_fn)fn;
    const struct dot_ctx *c = data;
    double acc = 0;
    for (int r = 0; r < reps; r++)
        acc += f(c->a, c->b, n);
    uint64_t bits;
    memcpy(&bits, &acc, sizeof bits);
    return bits;
}

/* Different summation orders legitimately give slightly different floats, so compare to a
   double-precision reference with a tolerance relative to the sum of |a*b|. */
static int verify_dot(const void *fn, const void *data, size_t n)
{
    const struct dot_ctx *c = data;
    double r = ((dot_fn)fn)(c->a, c->b, n);
    return fabs(r - c->ref) <= c->tol;
}

int run_dot_test(void)
{
    const struct impl impls[] = {
        {"C   -O0", (const void *)dot_c_O0, NULL, NULL},
        {"C   -O2", (const void *)dot_c_O2, NULL, NULL},
        {"C   -O3", (const void *)dot_c_O3, NULL, NULL},
        {"C   -O3 fast AVX2", (const void *)dot_c_O3fast, "AVX2/FMA", has_avx2_fma},
        {"asm AVX2+FMA", (const void *)dot_asm, "AVX2/FMA", has_avx2_fma},
    };

    static const size_t sizes[] = {(1u << 12) + 3, (1u << 18) + 3, (1u << 23) + 3};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        size_t n = sizes[s];
        float *a = alloc_bytes(n * sizeof *a), *b = alloc_bytes(n * sizeof *b);
        double ref = 0, abs_sum = 0;
        for (size_t i = 0; i < n; i++) {
            a[i] = ((int)(rng() % 2001) - 1000) / 1000.0f;
            b[i] = ((int)(rng() % 2001) - 1000) / 1000.0f;
            ref += (double)a[i] * b[i];
            abs_sum += fabs((double)a[i] * b[i]);
        }
        struct dot_ctx ctx = {a, b, ref, 1e-5 * abs_sum};

        char title[96];
        snprintf(title, sizeof title, "%zu floats per array (%.0f KiB for both)", n,
                 2.0 * n * sizeof *a / 1024.0);
        int rc = run_table(title, impls, 5, loop_dot, verify_dot, &ctx, n, 2.0 * n, 1.0, "GFLOP/s", 1);
        free(a);
        free(b);
        if (rc)
            return rc;
    }
    return 0;
}

/* ------------------------------------------------------------------ Mandelbrot */
typedef uint64_t (*mandel_fn)(int, int, int);

struct mandel_ctx {
    int w, h, max_iter;
};

static uint64_t loop_mandel(const void *fn, const void *data, size_t n, int reps)
{
    (void)n;
    mandel_fn f = (mandel_fn)fn;
    const struct mandel_ctx *c = data;
    uint64_t acc = 0;
    for (int r = 0; r < reps; r++)
        acc += f(c->w, c->h, c->max_iter);
    return acc;
}

int run_mandel_test(void)
{
    const struct impl impls[] = {
        {"C   -O0", (const void *)mandel_c_O0, NULL, NULL},
        {"C   -O2", (const void *)mandel_c_O2, NULL, NULL},
        {"C   -O3", (const void *)mandel_c_O3, NULL, NULL},
        {"asm SSE2 scalar", (const void *)mandel_asm_sse2, NULL, NULL},
        {"asm AVX2 4-wide", (const void *)mandel_asm_avx2, "AVX2", has_avx2},
    };
    struct mandel_ctx ctx = {400, 400, 500};
    uint64_t total = mandel_c_O0(ctx.w, ctx.h, ctx.max_iter);

    char title[96];
    snprintf(title, sizeof title, "%dx%d pixels, up to %d iterations each (%llu iterations total)",
             ctx.w, ctx.h, ctx.max_iter, (unsigned long long)total);
    /* units = iterations, scale 1e3 turns iterations/ns into millions of iterations/s */
    return run_table(title, impls, 5, loop_mandel, NULL, &ctx, 0, (double)total, 1e3, "Miter/s", 1);
}

/* ------------------------------------------------------------------ quicksort */
typedef void (*sort_fn)(int32_t *, size_t);

struct sort_ctx {
    const int32_t *src;      /* unsorted input */
    int32_t *work;           /* scratch copy that gets sorted */
    const int32_t *sorted;   /* expected result */
};

static uint64_t loop_sort(const void *fn, const void *data, size_t n, int reps)
{
    sort_fn f = (sort_fn)fn;
    const struct sort_ctx *c = data;
    uint64_t acc = 0;
    for (int r = 0; r < reps; r++) {
        memcpy(c->work, c->src, n * sizeof *c->work); /* identical copy cost for every row */
        f(c->work, n);
        acc += (uint32_t)c->work[0] + (uint32_t)c->work[n / 2] + (uint32_t)c->work[n - 1];
    }
    return acc;
}

static int verify_sort(const void *fn, const void *data, size_t n)
{
    const struct sort_ctx *c = data;
    memcpy(c->work, c->src, n * sizeof *c->work);
    ((sort_fn)fn)(c->work, n);
    return memcmp(c->work, c->sorted, n * sizeof *c->work) == 0;
}

static int cmp_i32(const void *x, const void *y)
{
    int32_t a = *(const int32_t *)x, b = *(const int32_t *)y;
    return (a > b) - (a < b);
}

static void libc_qsort(int32_t *a, size_t n) { qsort(a, n, sizeof *a, cmp_i32); }

int run_sort_test(void)
{
    const struct impl impls[] = {
        {"C   -O0", (const void *)sort_c_O0, NULL, NULL},
        {"C   -O2", (const void *)sort_c_O2, NULL, NULL},
        {"C   -O3", (const void *)sort_c_O3, NULL, NULL},
        {"asm", (const void *)sort_asm, NULL, NULL},
        {"libc qsort", (const void *)libc_qsort, NULL, NULL},
    };

    static const size_t sizes[] = {1u << 17, 1u << 20};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        size_t n = sizes[s];
        int32_t *src = alloc_bytes(n * 4), *work = alloc_bytes(n * 4), *sorted = alloc_bytes(n * 4);
        for (size_t i = 0; i < n; i++)
            src[i] = (int32_t)rng();
        memcpy(sorted, src, n * 4);
        qsort(sorted, n, 4, cmp_i32);
        struct sort_ctx ctx = {src, work, sorted};

        char title[64];
        snprintf(title, sizeof title, "%zu random int32 values (%.0f KiB)", n, n * 4 / 1024.0);
        /* units = keys, scale 1e3 turns keys/ns into millions of keys/s */
        int rc = run_table(title, impls, 5, loop_sort, verify_sort, &ctx, n, (double)n, 1e3, "Mkeys/s", 1);
        free(src);
        free(work);
        free(sorted);
        if (rc)
            return rc;
    }
    return 0;
}
