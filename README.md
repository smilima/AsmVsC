# NASM starter (x86-64 Linux, WSL)

| File | What it shows |
|---|---|
| `hello.asm` | Bare-metal style: `_start`, raw `write`/`exit` syscalls, linked with `ld` |
| `callc.asm` | Calling libc `printf` from `main`, System V calling convention, linked with `gcc` |

## Build / run / debug in VSCode
- **Ctrl+Shift+B**: build everything (`make`). NASM errors show in the Problems panel.
- **Terminal → Run Task → run current file**: builds, then runs the .asm file you have open.
- **F5**: debug the open .asm file in gdb. Click the gutter to set breakpoints on asm lines.
  Registers are in the Variables panel; right-click the editor → *Open Disassembly View* for the raw instructions.
  In the Debug Console, `-exec info registers` or `-exec x/16xb $rsp` work too.

## From the terminal
```bash
make            # build all *.asm into build/
./build/hello
make clean
objdump -d -M intel build/hello.o   # inspect the machine code
```
`build/<name>.lst` is the NASM listing (source next to the bytes it produced).

## Adding programs
Drop a new `.asm` file in this folder and run `make`. If it declares `global main` it is linked
with gcc (libc available; call functions as `call func wrt ..plt`), otherwise with ld and it
must define `_start`.

## Speed benchmark: asm vs C
`make bench` (about 45 s) builds `bench/`, times nine algorithms written in C (`-O0`/`-O2`/`-O3`) and in
hand-written NASM, checks every answer for correctness, and writes a text report next to the exe at
`build/bench_sum_report.txt` (the report starts with an explanation of what each table means).

| Test | Files | Asm technique |
|---|---|---|
| Array sum | `sum_*` | scalar and SSE2 |
| Prime sieve (primes to 1,000,000, sieves per 5 s) | `sieve_*` | `rep stosb`, SSE2 `psadbw` |
| FNV-1a hash | `fnv_*` | scalar (dependency-bound) |
| CRC-32C | `crc32_*` | SSE4.2 `crc32` instruction |
| SHA-256 | `sha256_*` | SHA-NI instructions |
| strlen | `strlen_*` | SSE2, 64 bytes per iteration |
| Dot product | `dot_*` | AVX2 + FMA |
| Mandelbrot | `mandel_*` | scalar SSE2 and 4-wide AVX2 |
| Quicksort | `sort_*` | scalar |

Rows that need a CPU feature the machine lacks (SSE4.2, SHA-NI, AVX2/FMA) are reported as skipped.
To add an algorithm: create `bench/<name>_c.c` (defines `FN`) and `bench/<name>_asm.asm`, add `<name>` to
`ALGOS` in the Makefile, then add a `run_<name>_test()` in `bench/bench_algos.c` and list it in `tests[]` in
`bench/bench.c`.
