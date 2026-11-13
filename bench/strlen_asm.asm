; strlen_asm.asm - SSE2 strlen:  size_t strlen_asm(const char *s)
; System V ABI: rdi = s, result in rax.
; Only 16-byte-aligned loads are used, so a read never crosses into an unmapped page
; (an aligned 16-byte block cannot straddle a page boundary). The main loop checks
; 64 bytes per iteration by taking the byte-wise minimum of four vectors.

        default rel

        section .text

        global  strlen_asm
strlen_asm:
        mov     rsi, rdi                ; remember start
        mov     ecx, edi
        and     ecx, 15                 ; misalignment of s
        and     rdi, -16                ; aligned block containing s
        pxor    xmm0, xmm0
        movdqa  xmm1, [rdi]
        pcmpeqb xmm1, xmm0
        pmovmskb eax, xmm1
        shr     eax, cl                 ; drop bytes that precede s
        test    eax, eax
        jz      .align
        bsf     eax, eax                ; NUL inside the first block
        ret

.align:                                 ; step 16 bytes at a time until 64-byte aligned
        add     rdi, 16
        test    dil, 63
        jz      .big
        movdqa  xmm1, [rdi]
        pcmpeqb xmm1, xmm0
        pmovmskb eax, xmm1
        test    eax, eax
        jz      .align
        jmp     .found

.big:                                   ; rdi is 64-byte aligned
        movdqa  xmm1, [rdi]
        movdqa  xmm2, [rdi + 16]
        movdqa  xmm3, [rdi + 32]
        movdqa  xmm4, [rdi + 48]
        pminub  xmm1, xmm2              ; a zero byte in any vector survives the minimum
        pminub  xmm3, xmm4
        pminub  xmm1, xmm3
        pcmpeqb xmm1, xmm0
        pmovmskb eax, xmm1
        test    eax, eax
        jnz     .locate
        add     rdi, 64
        jmp     .big

.locate:                                ; a NUL is somewhere in these 64 bytes
        movdqa  xmm1, [rdi]
        pcmpeqb xmm1, xmm0
        pmovmskb eax, xmm1
        test    eax, eax
        jnz     .found
        add     rdi, 16
        jmp     .locate

.found:
        bsf     eax, eax
        add     rax, rdi
        sub     rax, rsi
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
