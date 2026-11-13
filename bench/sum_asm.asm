; sum_asm.asm - two hand-written versions of: uint32_t sum(const uint32_t *a, size_t n)
; System V ABI: rdi = a, rsi = n, result in eax.

        default rel

        section .text

; Plain scalar loop, one element per iteration.
        global  sum_asm_scalar
sum_asm_scalar:
        xor     eax, eax
        test    rsi, rsi
        jz      .done
.loop:
        add     eax, [rdi]
        add     rdi, 4
        dec     rsi
        jnz     .loop
.done:
        ret

; SSE2 (baseline for x86-64): 16 elements per iteration, 4 independent accumulators.
        global  sum_asm_sse2
sum_asm_sse2:
        pxor    xmm0, xmm0
        pxor    xmm1, xmm1
        pxor    xmm2, xmm2
        pxor    xmm3, xmm3
.vloop:
        cmp     rsi, 16
        jb      .reduce
        movdqu  xmm4, [rdi]
        movdqu  xmm5, [rdi + 16]
        movdqu  xmm6, [rdi + 32]
        movdqu  xmm7, [rdi + 48]
        paddd   xmm0, xmm4
        paddd   xmm1, xmm5
        paddd   xmm2, xmm6
        paddd   xmm3, xmm7
        add     rdi, 64
        sub     rsi, 16
        jmp     .vloop
.reduce:
        paddd   xmm0, xmm1
        paddd   xmm2, xmm3
        paddd   xmm0, xmm2
        pshufd  xmm1, xmm0, 0x4E        ; swap 64-bit halves
        paddd   xmm0, xmm1
        pshufd  xmm1, xmm0, 0xB1        ; swap adjacent 32-bit lanes
        paddd   xmm0, xmm1
        movd    eax, xmm0
        test    rsi, rsi                ; 0..15 leftover elements
        jz      .done
.tail:
        add     eax, [rdi]
        add     rdi, 4
        dec     rsi
        jnz     .tail
.done:
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
