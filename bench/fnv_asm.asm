; fnv_asm.asm - FNV-1a, 64-bit:  uint64_t fnv_asm(const void *data, size_t n)
; System V ABI: rdi = data, rsi = n, result in rax.
; Every byte depends on the previous hash value, so there is nothing to vectorize.

        default rel

        section .text

        global  fnv_asm
fnv_asm:
        mov     rax, 0xcbf29ce484222325 ; offset basis
        mov     rcx, 0x100000001b3      ; FNV prime
        test    rsi, rsi
        jz      .done
.loop:
        movzx   edx, byte [rdi]
        xor     rax, rdx
        imul    rax, rcx
        inc     rdi
        dec     rsi
        jnz     .loop
.done:
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
