; dot_asm.asm - AVX2 + FMA dot product:
;   float dot_asm(const float *a, const float *b, size_t n)
; System V ABI: rdi = a, rsi = b, rdx = n, result in xmm0. Requires AVX2 and FMA.
; 32 floats per iteration with four independent accumulators (hides the FMA latency).

        default rel

        section .text

        global  dot_asm
dot_asm:
        vxorps  ymm0, ymm0, ymm0
        vxorps  ymm1, ymm1, ymm1
        vxorps  ymm2, ymm2, ymm2
        vxorps  ymm3, ymm3, ymm3
.vloop:
        cmp     rdx, 32
        jb      .reduce
        vmovups ymm4, [rdi]
        vmovups ymm5, [rdi + 32]
        vmovups ymm6, [rdi + 64]
        vmovups ymm7, [rdi + 96]
        vfmadd231ps ymm0, ymm4, [rsi]
        vfmadd231ps ymm1, ymm5, [rsi + 32]
        vfmadd231ps ymm2, ymm6, [rsi + 64]
        vfmadd231ps ymm3, ymm7, [rsi + 96]
        add     rdi, 128
        add     rsi, 128
        sub     rdx, 32
        jmp     .vloop
.reduce:
        vaddps  ymm0, ymm0, ymm1
        vaddps  ymm2, ymm2, ymm3
        vaddps  ymm0, ymm0, ymm2
        vextractf128 xmm1, ymm0, 1
        vaddps  xmm0, xmm0, xmm1        ; 8 lanes -> 4
        vmovhlps xmm1, xmm1, xmm0
        vaddps  xmm0, xmm0, xmm1        ; 4 -> 2
        vmovshdup xmm1, xmm0
        vaddss  xmm0, xmm0, xmm1        ; 2 -> 1
.tail:                                  ; 0..31 leftover elements
        test    rdx, rdx
        jz      .done
        vmovss  xmm1, [rdi]
        vfmadd231ss xmm0, xmm1, [rsi]
        add     rdi, 4
        add     rsi, 4
        dec     rdx
        jmp     .tail
.done:
        vzeroupper
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
