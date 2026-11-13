; sort_asm.asm - recursive quicksort on int32_t, same algorithm as sort_c.c
; (Hoare partition, middle pivot):
;   void sort_asm(int32_t *a, size_t n)
; System V ABI: rdi = a, rsi = n.

        default rel

        section .text

        global  sort_asm
sort_asm:
        cmp     rsi, 2
        jb      .ret
        lea     rdx, [rsi - 1]          ; hi = n - 1
        xor     esi, esi                ; lo = 0
        jmp     qs                      ; tail call
.ret:
        ret

; qs(rdi = a, rsi = lo, rdx = hi), signed indices. rdi is never modified.
qs:
        cmp     rsi, rdx
        jge     .leaf
        push    r12
        push    r13
        push    rbx                     ; 3 pushes + return address keep rsp 16-byte aligned
        mov     r12, rsi                ; lo
        mov     r13, rdx                ; hi

        mov     rax, rdx
        sub     rax, rsi
        shr     rax, 1
        add     rax, rsi
        mov     r8d, [rdi + rax * 4]    ; pivot = a[lo + (hi-lo)/2]
        lea     rcx, [rsi - 1]          ; i = lo - 1
        lea     rdx, [rdx + 1]          ; j = hi + 1
.scan_i:
        inc     rcx
        cmp     [rdi + rcx * 4], r8d
        jl      .scan_i
.scan_j:
        dec     rdx
        cmp     [rdi + rdx * 4], r8d
        jg      .scan_j
        cmp     rcx, rdx
        jge     .partitioned
        mov     eax, [rdi + rcx * 4]
        mov     r9d, [rdi + rdx * 4]
        mov     [rdi + rcx * 4], r9d
        mov     [rdi + rdx * 4], eax
        jmp     .scan_i

.partitioned:                           ; rdx = j
        mov     rbx, rdx
        mov     rsi, r12
        call    qs                      ; qs(a, lo, j)
        lea     rsi, [rbx + 1]
        mov     rdx, r13
        call    qs                      ; qs(a, j + 1, hi)
        pop     rbx
        pop     r13
        pop     r12
.leaf:
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
