; sha256_asm.asm - SHA-256 block compression using the Intel SHA extensions (SHA-NI):
;   void sha256_asm(uint32_t state[8], const uint8_t *data, size_t nblocks)
; System V ABI: rdi = state, rsi = data, rdx = nblocks (whole 64-byte blocks, no padding).
; Requires SHA-NI (plus SSSE3/SSE4.1). Same contract as sha256_c.c.
;
; Register use:
;   xmm1 = STATE0 (ABEF)   xmm2 = STATE1 (CDGH)   xmm0 = message+K (implicit operand of sha256rnds2)
;   xmm3..xmm6 = rolling message schedule W[i%4]   xmm7 = temp
;   xmm8/xmm9 = state saved at block start         xmm10 = byte-swap mask

        default rel

        section .rodata align=16
bswap_mask:
        db      3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12
k256:
        dd      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
        dd      0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
        dd      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
        dd      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
        dd      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
        dd      0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
        dd      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
        dd      0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
        dd      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
        dd      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
        dd      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
        dd      0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
        dd      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
        dd      0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
        dd      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
        dd      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2

; Four rounds, using W group `cur`. With W[i] = message words 4i..4i+3:
;   %1 = cur (W[i])  %2 = prev (W[i-1])  %3 = next (W[i+1], gets finished here)
;   %4 = byte offset of this group's K constants
;   %5 = 1 to finish W[i+1] (sha256msg2), needs 3 <= i <= 14
;   %6 = 1 to start  W[i]'s successor term (sha256msg1), needs 1 <= i <= 12
%macro ROUNDS4 6
        movdqa  xmm0, %1
        paddd   xmm0, [k256 + %4]
        sha256rnds2 xmm2, xmm1, xmm0
%if %5
        movdqa  xmm7, %1
        palignr xmm7, %2, 4             ; W[i-1][1..3], W[i][0]
        paddd   %3, xmm7
        sha256msg2 %3, %1
%endif
        pshufd  xmm0, xmm0, 0x0E        ; move the upper two words down for rounds 3-4
        sha256rnds2 xmm1, xmm2, xmm0
%if %6
        sha256msg1 %2, %1
%endif
%endmacro

        section .text

        global  sha256_asm
sha256_asm:
        test    rdx, rdx
        jz      .ret

        movdqu  xmm1, [rdi]             ; state[0..3] = A B C D
        movdqu  xmm2, [rdi + 16]        ; state[4..7] = E F G H
        pshufd  xmm7, xmm1, 0xB1        ; C D A B
        pshufd  xmm2, xmm2, 0x1B        ; E F G H (reversed lane order)
        movdqa  xmm1, xmm7
        palignr xmm1, xmm2, 8           ; A B E F
        pblendw xmm2, xmm7, 0xF0        ; C D G H
        movdqa  xmm10, [bswap_mask]

.block:
        movdqa  xmm8, xmm1
        movdqa  xmm9, xmm2

        movdqu  xmm3, [rsi]
        movdqu  xmm4, [rsi + 16]
        movdqu  xmm5, [rsi + 32]
        movdqu  xmm6, [rsi + 48]
        pshufb  xmm3, xmm10             ; big-endian words
        pshufb  xmm4, xmm10
        pshufb  xmm5, xmm10
        pshufb  xmm6, xmm10

        ;       cur   prev  next  K      msg2 msg1
        ROUNDS4 xmm3, xmm6, xmm4, 0,     0,   0
        ROUNDS4 xmm4, xmm3, xmm5, 16,    0,   1
        ROUNDS4 xmm5, xmm4, xmm6, 32,    0,   1
        ROUNDS4 xmm6, xmm5, xmm3, 48,    1,   1
        ROUNDS4 xmm3, xmm6, xmm4, 64,    1,   1
        ROUNDS4 xmm4, xmm3, xmm5, 80,    1,   1
        ROUNDS4 xmm5, xmm4, xmm6, 96,    1,   1
        ROUNDS4 xmm6, xmm5, xmm3, 112,   1,   1
        ROUNDS4 xmm3, xmm6, xmm4, 128,   1,   1
        ROUNDS4 xmm4, xmm3, xmm5, 144,   1,   1
        ROUNDS4 xmm5, xmm4, xmm6, 160,   1,   1
        ROUNDS4 xmm6, xmm5, xmm3, 176,   1,   1
        ROUNDS4 xmm3, xmm6, xmm4, 192,   1,   1
        ROUNDS4 xmm4, xmm3, xmm5, 208,   1,   0
        ROUNDS4 xmm5, xmm4, xmm6, 224,   1,   0
        ROUNDS4 xmm6, xmm5, xmm3, 240,   0,   0

        paddd   xmm1, xmm8
        paddd   xmm2, xmm9
        add     rsi, 64
        dec     rdx
        jnz     .block

        pshufd  xmm7, xmm1, 0x1B        ; F E B A
        pshufd  xmm2, xmm2, 0xB1        ; D C H G
        movdqa  xmm1, xmm7
        pblendw xmm1, xmm2, 0xF0        ; D C B A
        palignr xmm2, xmm7, 8           ; H G F E
        movdqu  [rdi], xmm1
        movdqu  [rdi + 16], xmm2
.ret:
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
