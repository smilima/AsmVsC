; hello.asm - x86-64 Linux, no libc. Entry point is _start; talks to the kernel directly.
; Build:  make            Run:  ./build/hello

        default rel                     ; RIP-relative addressing by default

        section .rodata
msg:    db      "Hello from NASM on WSL!", 10
msglen: equ     $ - msg

        section .text
        global  _start
_start:
        mov     eax, 1                  ; sys_write
        mov     edi, 1                  ; fd 1 = stdout
        lea     rsi, [msg]              ; buffer
        mov     edx, msglen             ; length
        syscall

        mov     eax, 60                 ; sys_exit
        xor     edi, edi                ; status 0
        syscall

        section .note.GNU-stack noalloc noexec nowrite progbits   ; non-executable stack
