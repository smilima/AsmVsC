; mandel_asm.asm - Mandelbrot escape-time totals, same contract as mandel_c.c:
;   uint64_t mandel_asm_sse2(int w, int h, int max_iter)   scalar double, any w
;   uint64_t mandel_asm_avx2(int w, int h, int max_iter)   4 pixels at once, w multiple of 4
; System V ABI: edi = w, esi = h, edx = max_iter, total iteration count in rax.
; No FMA is used, so rounding (and therefore the result) is identical to the C version.

        default rel

        section .rodata
        align   8
c_three: dq     3.0
c_four: dq      4.0
c_xmin: dq      -2.0
c_ymin: dq      -1.5
        align   16
lane_idx: dd    0, 1, 2, 3

        section .text

; ---------------------------------------------------------------- scalar SSE2
        global  mandel_asm_sse2
mandel_asm_sse2:
        movsd   xmm8, [c_three]
        cvtsi2sd xmm0, edi
        divsd   xmm8, xmm0              ; dx = 3.0 / w
        movsd   xmm9, [c_three]
        cvtsi2sd xmm0, esi
        divsd   xmm9, xmm0              ; dy = 3.0 / h
        movsd   xmm10, [c_four]
        movsd   xmm11, [c_xmin]
        movsd   xmm12, [c_ymin]

        xor     eax, eax                ; total
        xor     r10d, r10d              ; y
.row:
        cmp     r10d, esi
        jge     .done
        cvtsi2sd xmm13, r10d
        mulsd   xmm13, xmm9
        addsd   xmm13, xmm12            ; ci = -1.5 + y*dy
        xor     ecx, ecx                ; x
.col:
        cmp     ecx, edi
        jge     .next_row
        cvtsi2sd xmm14, ecx
        mulsd   xmm14, xmm8
        addsd   xmm14, xmm11            ; cr = -2.0 + x*dx
        xorpd   xmm0, xmm0              ; zr
        xorpd   xmm1, xmm1              ; zi
        xor     r11d, r11d              ; it
.iter:
        cmp     r11d, edx
        jge     .iter_done
        movapd  xmm2, xmm0
        mulsd   xmm2, xmm2              ; zr*zr
        movapd  xmm3, xmm1
        mulsd   xmm3, xmm3              ; zi*zi
        movapd  xmm4, xmm2
        addsd   xmm4, xmm3
        ucomisd xmm4, xmm10
        ja      .iter_done              ; zr*zr + zi*zi > 4.0: escaped
        addsd   xmm0, xmm0              ; 2*zr
        mulsd   xmm0, xmm1
        addsd   xmm0, xmm13             ; t = 2*zr*zi + ci
        subsd   xmm2, xmm3
        addsd   xmm2, xmm14             ; zr = zr*zr - zi*zi + cr
        movapd  xmm1, xmm0              ; zi = t
        movapd  xmm0, xmm2
        inc     r11d
        jmp     .iter
.iter_done:
        add     rax, r11
        inc     ecx
        jmp     .col
.next_row:
        inc     r10d
        jmp     .row
.done:
        ret

; ------------------------------------------------------------------- AVX2
        global  mandel_asm_avx2
mandel_asm_avx2:
        vmovsd  xmm8, [c_three]
        vcvtsi2sd xmm0, xmm0, edi
        vdivsd  xmm8, xmm8, xmm0        ; dx
        vbroadcastsd ymm8, xmm8
        vmovsd  xmm9, [c_three]
        vcvtsi2sd xmm0, xmm0, esi
        vdivsd  xmm9, xmm9, xmm0        ; dy (scalar)
        vbroadcastsd ymm10, [c_four]
        vbroadcastsd ymm11, [c_xmin]
        vmovsd  xmm12, [c_ymin]

        xor     eax, eax                ; total
        xor     r10d, r10d              ; y
.row:
        cmp     r10d, esi
        jge     .done
        vcvtsi2sd xmm13, xmm13, r10d
        vmulsd  xmm13, xmm13, xmm9
        vaddsd  xmm13, xmm13, xmm12     ; ci
        vbroadcastsd ymm13, xmm13
        xor     ecx, ecx                ; x of the first pixel in the group
.col:
        cmp     ecx, edi
        jge     .next_row
        vmovd   xmm6, ecx
        vpbroadcastd xmm6, xmm6
        vpaddd  xmm6, xmm6, [lane_idx]
        vcvtdq2pd ymm14, xmm6           ; x, x+1, x+2, x+3 as doubles
        vmulpd  ymm14, ymm14, ymm8
        vaddpd  ymm14, ymm14, ymm11     ; cr per lane
        vxorpd  ymm0, ymm0, ymm0        ; zr
        vxorpd  ymm1, ymm1, ymm1        ; zi
        vpxor   ymm2, ymm2, ymm2        ; per-lane iteration counts (int64)
        vpcmpeqd ymm5, ymm5, ymm5       ; active mask = all ones
        xor     r11d, r11d              ; it
.iter:
        cmp     r11d, edx
        jge     .iter_done
        vmulpd  ymm3, ymm0, ymm0        ; zr*zr
        vmulpd  ymm4, ymm1, ymm1        ; zi*zi
        vaddpd  ymm6, ymm3, ymm4
        vcmppd  ymm6, ymm6, ymm10, 2    ; |z|^2 <= 4.0
        vandpd  ymm5, ymm5, ymm6        ; lanes stay inactive once escaped
        vmovmskpd r8d, ymm5
        test    r8d, r8d
        jz      .iter_done              ; every lane escaped
        vpsubq  ymm2, ymm2, ymm5        ; count += 1 in active lanes (mask is -1)
        vaddpd  ymm7, ymm0, ymm0
        vmulpd  ymm7, ymm7, ymm1
        vaddpd  ymm7, ymm7, ymm13       ; t = 2*zr*zi + ci
        vsubpd  ymm3, ymm3, ymm4
        vaddpd  ymm0, ymm3, ymm14       ; zr = zr*zr - zi*zi + cr
        vmovapd ymm1, ymm7              ; zi = t
        inc     r11d
        jmp     .iter
.iter_done:
        vextracti128 xmm3, ymm2, 1
        vpaddq  xmm2, xmm2, xmm3
        vpshufd xmm3, xmm2, 0x4E
        vpaddq  xmm2, xmm2, xmm3
        vmovq   r8, xmm2
        add     rax, r8
        add     ecx, 4
        jmp     .col
.next_row:
        inc     r10d
        jmp     .row
.done:
        vzeroupper
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
