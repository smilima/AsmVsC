#ifndef BENCH_H
#define BENCH_H

/* printf to the console and, if open, to the report file. */
void emit(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
double now_ns(void);

/* Each returns 0 on success, nonzero if an implementation gave a wrong answer. */
int run_fnv_test(void);
int run_crc32_test(void);
int run_sha256_test(void);
int run_strlen_test(void);
int run_dot_test(void);
int run_mandel_test(void);
int run_sort_test(void);

#endif
