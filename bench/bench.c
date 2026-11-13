/* Benchmark harness: compares hand-written NASM against C compiled at several -O levels.
   Build/run:  make bench   (from the project root) */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bench.h"

typedef uint32_t (*sum_fn)(const uint32_t *, size_t);

uint32_t sum_c_O0(const uint32_t *, size_t);
uint32_t sum_c_O2(const uint32_t *, size_t);
uint32_t sum_c_O3(const uint32_t *, size_t);
uint32_t sum_asm_scalar(const uint32_t *, size_t);
uint32_t sum_asm_sse2(const uint32_t *, size_t);

typedef uint32_t (*sieve_fn)(uint8_t *, uint32_t);

uint32_t sieve_c_O0(uint8_t *, uint32_t);
uint32_t sieve_c_O2(uint8_t *, uint32_t);
uint32_t sieve_c_O3(uint8_t *, uint32_t);
uint32_t sieve_asm(uint8_t *, uint32_t);

#define SIEVE_LIMIT 1000000u
#define SIEVE_PRIMES 78498u /* number of primes <= 1,000,000 */
#define SIEVE_SECONDS 5.0

static volatile uint32_t sink; /* keeps the calls from being optimized away */
static FILE *report;           /* NULL if the report file could not be created */

void emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (report) {
        va_list copy;
        va_copy(copy, ap);
        vfprintf(report, fmt, copy);
        va_end(copy);
    }
    vprintf(fmt, ap);
    va_end(ap);
}

/* Opens "<path of this executable>_report.txt" so the report sits next to the exe. */
static void open_report(char *path, size_t size)
{
    ssize_t len = readlink("/proc/self/exe", path, size - 1);
    if (len < 0) {
        path[0] = '\0';
        return;
    }
    path[len] = '\0';
    if (strlen(path) + sizeof "_report.txt" > size) {
        path[0] = '\0';
        return;
    }
    strcat(path, "_report.txt");
    report = fopen(path, "w");
}

static void cpu_model(char *out, size_t size)
{
    snprintf(out, size, "unknown");
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *colon = strstr(line, "model name");
        if (colon && (colon = strchr(line, ':'))) {
            snprintf(out, size, "%s", colon + 2);
            out[strcspn(out, "\n")] = '\0';
            break;
        }
    }
    fclose(f);
}

double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

/* Returns the best (minimum) time in ns for one call of fn over n elements.
   Takes the best of `trials` runs of `reps` calls each, to filter out scheduler noise. */
static double bench(sum_fn fn, const uint32_t *data, size_t n, int reps, int trials)
{
    double best = 1e30;
    for (int t = 0; t < trials; t++) {
        double start = now_ns();
        uint32_t acc = 0;
        for (int r = 0; r < reps; r++)
            acc += fn(data, n);
        double per_call = (now_ns() - start) / reps;
        sink = acc;
        if (per_call < best)
            best = per_call;
    }
    return best;
}

static int run_sum_test(void)
{
    static const struct {
        const char *name;
        sum_fn fn;
    } impls[] = {
        {"C   -O0",       sum_c_O0},
        {"C   -O2",       sum_c_O2},
        {"C   -O3",       sum_c_O3},
        {"asm scalar",    sum_asm_scalar},
        {"asm SSE2",      sum_asm_sse2},
    };
    enum { NIMPL = sizeof impls / sizeof impls[0] };

    /* ~16 KiB (L1), ~1 MiB (L2), ~64 MiB (RAM) of uint32_t; the +3 exercises the tail loops. */
    static const size_t sizes[] = {(1u << 12) + 3, (1u << 18) + 3, (1u << 24) + 3};

    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        size_t n = sizes[s];
        uint32_t *data = malloc(n * sizeof *data);
        if (!data)
            return 1;
        for (size_t i = 0; i < n; i++)
            data[i] = (uint32_t)(i * 2654435761u);

        uint32_t expected = sum_c_O0(data, n);
        int reps = (int)((1u << 26) / n) + 1; /* ~64M elements per trial */

        emit("\n%zu elements (%.0f KiB)\n", n, n * sizeof *data / 1024.0);
        emit("  %-12s %12s %10s %9s\n", "impl", "ns/call", "GB/s", "vs C -O2");

        double base = 0;
        for (int i = 0; i < NIMPL; i++) {
            if (impls[i].fn(data, n) != expected) {
                emit("  %-12s WRONG RESULT\n", impls[i].name);
                free(data);
                return 1;
            }
            double ns = bench(impls[i].fn, data, n, reps, 5);
            if (i == 1)
                base = ns;
            emit("  %-12s %12.0f %10.2f", impls[i].name, ns, n * sizeof *data / ns);
            if (base > 0)
                emit(" %8.2fx", base / ns);
            emit("\n");
        }
        free(data);
    }
    return 0;
}

/* Runs fn back to back until `seconds` have elapsed; returns how many complete sieves ran
   and stores the exact elapsed time in *elapsed_s. */
static uint64_t count_sieves(sieve_fn fn, uint8_t *buf, double seconds, double *elapsed_s)
{
    uint64_t runs = 0;
    uint32_t acc = 0;
    double start = now_ns(), now;
    do {
        acc += fn(buf, SIEVE_LIMIT);
        runs++;
        now = now_ns();
    } while (now - start < seconds * 1e9);
    sink = acc;
    *elapsed_s = (now - start) / 1e9;
    return runs;
}

static int run_sieve_test(void)
{
    static const struct {
        const char *name;
        sieve_fn fn;
    } impls[] = {
        {"C   -O0", sieve_c_O0},
        {"C   -O2", sieve_c_O2},
        {"C   -O3", sieve_c_O3},
        {"asm",     sieve_asm},
    };
    enum { NIMPL = sizeof impls / sizeof impls[0] };

    uint8_t *buf = malloc(SIEVE_LIMIT + 1);
    if (!buf)
        return 1;

    emit("\nPrimes up to %u (expect %u), %.0f seconds per implementation\n", SIEVE_LIMIT,
         SIEVE_PRIMES, SIEVE_SECONDS);
    emit("  %-12s %12s %12s %10s %9s\n", "impl", "sieves run", "sieves/sec", "ms/sieve",
         "vs C -O2");

    double base = 0;
    for (int i = 0; i < NIMPL; i++) {
        uint32_t primes = impls[i].fn(buf, SIEVE_LIMIT);
        if (primes != SIEVE_PRIMES) {
            emit("  %-12s WRONG RESULT (%u primes)\n", impls[i].name, primes);
            free(buf);
            return 1;
        }
        double secs;
        uint64_t runs = count_sieves(impls[i].fn, buf, SIEVE_SECONDS, &secs);
        double per_sec = runs / secs;
        if (i == 1)
            base = per_sec;
        emit("  %-12s %12llu %12.1f %10.3f", impls[i].name, (unsigned long long)runs, per_sec,
             1000.0 / per_sec);
        if (base > 0)
            emit(" %8.2fx", per_sec / base);
        emit("\n");
        fflush(stdout);
    }
    free(buf);
    return 0;
}

static const char intro[] =
    "WHAT THIS SHOWS\n"
    "---------------\n"
    "Nine small programs are each written in C (compiled at -O0, -O2 and -O3) and in\n"
    "hand-written NASM assembly, then timed, to see how much faster (or slower)\n"
    "assembly is than what the C compiler produces. Every implementation's answer is\n"
    "checked for correctness before it is timed. A row marked 'skipped' needs a CPU\n"
    "feature this machine does not have.\n"
    "\n"
    "  C   -O0      no optimization\n"
    "  C   -O2      gcc -O2\n"
    "  C   -O3      gcc -O3 (also auto-vectorizes with SIMD where it can)\n"
    "  asm ...      hand-written NASM\n"
    "\n"
    "All tables share the same columns: 'ns/call' is the time for one call (lower is\n"
    "better), the middle column is throughput in the unit named in its header (higher is\n"
    "better), and 'vs C -O2' is speedup relative to the C -O2 row (>1.00x is faster,\n"
    "<1.00x is slower; blank for rows before the baseline). Unless noted, each row is\n"
    "the best of 5 trials. Results vary with the CPU and system load, so compare rows\n"
    "within one report rather than across machines.\n"
    "\n"
    "TEST 1: ARRAY SUM\n"
    "  Adds up an array of 32-bit unsigned integers. Two asm versions: 'asm scalar' (one\n"
    "  element per iteration) and 'asm SSE2' (SIMD, 16 elements per iteration). Array\n"
    "  sizes fit L1 cache (~16 KiB), L2 cache (~1 MiB) and main RAM (~64 MiB).\n"
    "  Reading it: C -O0 is far slower than the rest. Scalar asm only matches C -O2,\n"
    "  since the compiler already generates a similar loop. The big gains come from\n"
    "  SIMD (C -O3 and asm SSE2). At RAM-sized arrays every fast version is limited by\n"
    "  memory bandwidth, so the differences shrink.\n"
    "\n"
    "TEST 2: PRIME SIEVE\n"
    "  Sieve of Eratosthenes: finds and counts all primes up to 1,000,000 (there are\n"
    "  78,498). Each implementation repeats the complete sieve back to back for 5\n"
    "  seconds and we count how many full sieves it finished ('sieves run', 'sieves/sec',\n"
    "  'ms/sieve'). Same algorithm in C and asm.\n"
    "  Reading it: the time is dominated by the strided byte writes that cross out\n"
    "  multiples, which no SIMD can speed up. The asm uses 'rep stosb' to fill the\n"
    "  table and SSE2 (psadbw) to count the primes; expect it to land within a few\n"
    "  percent of C -O2/-O3. C -O0 is the outlier, several times slower.\n"
    "\n"
    "TEST 3: FNV-1a HASH (64-bit)\n"
    "  Hashes a byte buffer. Each byte depends on the previous hash value, so the work\n"
    "  is one long dependency chain (xor, then multiply) that cannot be vectorized.\n"
    "  Reading it: hand-written asm has nothing to add over the compiler here; the\n"
    "  speed is set by the multiply latency, so C -O2/-O3 and asm should tie.\n"
    "\n"
    "TEST 4: CRC-32C CHECKSUM\n"
    "  Checks a byte buffer with the CRC-32C polynomial. 'C table' is the classic\n"
    "  portable one-byte-per-step table lookup. 'C intrinsics' and 'asm crc32 instr' use\n"
    "  the CPU's dedicated SSE4.2 crc32 instruction (8 bytes per instruction). The\n"
    "  intrinsics row shows that C can match asm once it is allowed to use the\n"
    "  instruction; the compiler will not turn the table loop into it by itself.\n"
    "  (A faster pclmulqdq / multi-stream version is not included.)\n"
    "\n"
    "TEST 5: SHA-256\n"
    "  The SHA-256 block compression function over 1 MiB (no padding). The C versions are\n"
    "  a portable implementation; 'asm SHA-NI' uses the CPU's SHA extension instructions\n"
    "  (sha256rnds2, sha256msg1, sha256msg2), which the compiler never generates for\n"
    "  plain C. Verified against the published digest of \"abc\".\n"
    "\n"
    "TEST 6: STRLEN\n"
    "  Measures the length of a string of 31, 4,095 and ~1 million characters. The C\n"
    "  versions are the naive byte loop (built with -fno-tree-loop-distribute-patterns:\n"
    "  otherwise gcc -O2/-O3 recognizes the loop and silently replaces it with a call to\n"
    "  libc's strlen). 'asm SSE2' compares 16 bytes at a time (64 per iteration in the\n"
    "  main loop) using aligned loads so it never reads across a page. 'libc strlen' is\n"
    "  glibc's own hand-tuned version (wider vectors), included for context. Verified for\n"
    "  every start alignment 0-63 and lengths 0-200.\n"
    "\n"
    "TEST 7: DOT PRODUCT (float)\n"
    "  Multiply-adds two float arrays. Strict floating-point rules forbid reordering the\n"
    "  sum, so plain C -O2/-O3 stay scalar. 'C -O3 fast AVX2' is rebuilt with\n"
    "  -O3 -mavx2 -mfma -ffast-math, which lets the compiler vectorize; 'asm AVX2+FMA'\n"
    "  does it by hand. The summation order differs between rows, so results are checked\n"
    "  against a double-precision reference within a small tolerance, not bit-for-bit.\n"
    "\n"
    "TEST 8: MANDELBROT\n"
    "  Counts escape-time iterations over a 400x400 grid (double precision). Unit is\n"
    "  millions of iterations per second. 'asm SSE2 scalar' does one pixel at a time like\n"
    "  the C code; 'asm AVX2 4-wide' iterates four pixels at once, masking out pixels as\n"
    "  they escape. No FMA is used, so all rows return exactly the same totals.\n"
    "\n"
    "TEST 9: QUICKSORT\n"
    "  Sorts random 32-bit integers with the same recursive Hoare-partition quicksort in\n"
    "  C and asm. Unit is millions of keys sorted per second; every call includes the\n"
    "  same copy of the input. 'libc qsort' (generic, comparison via callback) is shown\n"
    "  for context. Sorting is branchy and data-dependent, so there is little for\n"
    "  hand-written asm to gain over the compiler.\n";

static const struct {
    const char *title;
    int (*run)(void);
    const char *note;
} tests[] = {
    {"TEST 1: ARRAY SUM", run_sum_test, "best of 5 trials"},
    {"TEST 2: PRIME SIEVE", run_sieve_test, "5 s per implementation"},
    {"TEST 3: FNV-1a HASH", run_fnv_test, "best of 5 trials"},
    {"TEST 4: CRC-32C", run_crc32_test, "best of 5 trials"},
    {"TEST 5: SHA-256", run_sha256_test, "best of 5 trials"},
    {"TEST 6: STRLEN", run_strlen_test, "best of 5 trials"},
    {"TEST 7: DOT PRODUCT", run_dot_test, "best of 5 trials"},
    {"TEST 8: MANDELBROT", run_mandel_test, "best of 5 trials"},
    {"TEST 9: QUICKSORT", run_sort_test, "best of 5 trials"},
};

int main(void)
{
    char path[PATH_MAX];
    open_report(path, sizeof path);

    char cpu[128], stamp[64];
    time_t t = time(NULL);
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&t));
    cpu_model(cpu, sizeof cpu);
    emit("asm vs C speed report\n");
    emit("date: %s\ncpu:  %s\n", stamp, cpu);
    emit("C compiler: gcc %s\n\n", __VERSION__);
    emit("%s", intro);

    int rc = 0;
    for (size_t i = 0; i < sizeof tests / sizeof tests[0] && rc == 0; i++) {
        char heading[128];
        snprintf(heading, sizeof heading, "RESULTS: %s (%s)", tests[i].title, tests[i].note);
        emit("\n%s\n", heading);
        for (size_t k = 0; heading[k]; k++)
            emit("-");
        emit("\n");
        printf("(running %s...)\n", tests[i].title);
        fflush(stdout);
        rc = tests[i].run();
    }

    if (report) {
        fclose(report);
        printf("\nreport written to %s\n", path);
    }
    return rc;
}
