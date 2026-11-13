; callc.asm - x86-64 Linux, calling into libc (printf). Entry point is main,
; so the Makefile links it with gcc instead of ld.
; Build:  make            Run:  ./build/callc

        default rel

        extern  printf

        section .rodata
fmt:    db      "%d squared is %d", 10, 0

        section .text
        global  main
main:
        push    rbx                     ; callee-saved; also re-aligns the stack to 16 bytes

        mov     ebx, 1                  ; loop counter
.loop:
        mov     edx, ebx
        imul    edx, ebx                ; 3rd arg (rdx) = n*n
        mov     esi, ebx                ; 2nd arg (rsi) = n
        lea     rdi, [fmt]              ; 1st arg (rdi) = format string
        xor     eax, eax                ; al = number of vector regs used (varargs rule)
        call    printf wrt ..plt        ; via PLT so it works in a PIE executable

        inc     ebx
        cmp     ebx, 5
        jle     .loop

        xor     eax, eax                ; return 0
        pop     rbx
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
