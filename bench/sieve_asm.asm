; sieve_asm.asm - hand-written sieve of Eratosthenes:
;   uint32_t sieve_asm(uint8_t *buf, uint32_t limit)
; System V ABI: rdi = buf (limit+1 bytes), esi = limit (>= 2). Returns prime count in eax.
; Same algorithm as sieve_c.c: byte flags, 1 = "still looks prime".

        default rel

        section .text

        global  sieve_asm
sieve_asm:
        mov     r8, rdi                 ; r8 = buf (rep stosb clobbers rdi)
        mov     r9d, esi                ; r9 = limit
        lea     rcx, [r9 + 1]
        mov     al, 1
        rep stosb                       ; buf[0..limit] = 1
        mov     byte [r8], 0
        mov     byte [r8 + 1], 0

        mov     ecx, 2                  ; rcx = p
.outer:
        mov     rax, rcx
        imul    rax, rax                ; rax = p*p
        cmp     rax, r9
        ja      .count                  ; p*p > limit: sieving done
        cmp     byte [r8 + rcx], 0
        je      .next                   ; p is composite
.inner:
        mov     byte [r8 + rax], 0      ; cross out multiple of p
        add     rax, rcx
        cmp     rax, r9
        jbe     .inner
.next:
        inc     rcx
        jmp     .outer

; Count the 1 bytes in buf[2..limit]: 16 bytes at a time with psadbw (sum of bytes vs. zero).
.count:
        pxor    xmm0, xmm0              ; running sums (two 64-bit lanes)
        pxor    xmm2, xmm2              ; zero
        mov     ecx, 2                  ; rcx = index
        lea     rdx, [r9 + 1]           ; rdx = end (exclusive)
.vloop:
        lea     rax, [rcx + 16]
        cmp     rax, rdx
        ja      .vdone                  ; fewer than 16 bytes left
        movdqu  xmm1, [r8 + rcx]
        psadbw  xmm1, xmm2
        paddq   xmm0, xmm1
        mov     rcx, rax
        jmp     .vloop
.vdone:
        pshufd  xmm1, xmm0, 0x4E
        paddq   xmm0, xmm1
        movd    eax, xmm0
.tail:
        cmp     rcx, rdx
        jae     .done
        movzx   esi, byte [r8 + rcx]
        add     eax, esi
        inc     rcx
        jmp     .tail
.done:
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
