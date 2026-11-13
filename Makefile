# Builds every *.asm in this folder into build/<name>.
#   Files that declare `global main` are linked with gcc (so you get libc).
#   Everything else is linked with ld and must define _start.
# Drop a new .asm file in here and `make` picks it up automatically.

ASM      := nasm
ASMFLAGS := -f elf64 -g -F dwarf -w+all
BUILD    := build

SRCS := $(wildcard *.asm)
BINS := $(SRCS:%.asm=$(BUILD)/%)

.PHONY: all clean bench
.SECONDARY:                      # keep the .o files for objdump/inspection

all: $(BINS)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.asm | $(BUILD)
	$(ASM) $(ASMFLAGS) -l $(BUILD)/$*.lst -o $@ $<

$(BUILD)/%: $(BUILD)/%.o
	@if grep -qE '^[[:space:]]*global[[:space:]]+main\b' $*.asm; then \
		echo "gcc -g -o $@ $<"; gcc -g -o $@ $<; \
	else \
		echo "ld -o $@ $<"; ld -o $@ $<; \
	fi

# Speed comparison: hand-written asm vs C (see bench/). Run with `make bench`.
# Each algorithm <a> has bench/<a>_c.c (built at -O0/-O2/-O3, symbol <a>_c_<level>) and
# bench/<a>_asm.asm. Extra C variants: dot_c_O3fast (-ffast-math AVX2) and crc32_c_hw (intrinsics).
BENCH_BIN := $(BUILD)/bench_sum
# gcc would otherwise replace the naive strlen loop with a call to libc's strlen
CFLAGS_strlen := -fno-tree-loop-distribute-patterns
ALGOS     := sum sieve fnv crc32 sha256 strlen dot mandel sort
BENCH_OBJS := $(BUILD)/bench.o $(BUILD)/bench_algos.o \
	$(foreach a,$(ALGOS),$(BUILD)/$(a)_asm.o $(foreach o,O0 O2 O3,$(BUILD)/$(a)_c_$(o).o)) \
	$(BUILD)/dot_c_O3fast.o $(BUILD)/crc32_c_hw.o

$(BUILD)/%_c_O0.o: bench/%_c.c | $(BUILD)
	$(CC) -O0 $(CFLAGS_$*) -DFN=$*_c_O0 -c -o $@ $<

$(BUILD)/%_c_O2.o: bench/%_c.c | $(BUILD)
	$(CC) -O2 $(CFLAGS_$*) -DFN=$*_c_O2 -c -o $@ $<

$(BUILD)/%_c_O3.o: bench/%_c.c | $(BUILD)
	$(CC) -O3 $(CFLAGS_$*) -DFN=$*_c_O3 -c -o $@ $<

$(BUILD)/%_c_O3fast.o: bench/%_c.c | $(BUILD)
	$(CC) -O3 -mavx2 -mfma -ffast-math -DFN=$*_c_O3fast -c -o $@ $<

$(BUILD)/crc32_c_hw.o: bench/crc32_c.c | $(BUILD)
	$(CC) -O2 -msse4.2 -DHW -DFN=crc32_c_hw -c -o $@ $<

$(BUILD)/%_asm.o: bench/%_asm.asm | $(BUILD)
	$(ASM) $(ASMFLAGS) -w-reloc-rel-dword -o $@ $<

$(BUILD)/bench.o $(BUILD)/bench_algos.o: $(BUILD)/%.o: bench/%.c bench/bench.h | $(BUILD)
	$(CC) -O2 -Wall -Wextra -c -o $@ $<

$(BENCH_BIN): $(BENCH_OBJS)
	$(CC) -o $@ $^ -lm

bench: $(BENCH_BIN)
	./$(BENCH_BIN)

clean:
	rm -rf $(BUILD)
