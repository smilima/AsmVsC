; crc32_asm.asm - CRC-32C using the SSE4.2 hardware crc32 instruction:
;   uint32_t crc32_asm(const void *data, size_t n)
; System V ABI: rdi = data, rsi = n, result in eax. Requires SSE4.2.
; Same CRC as crc32_c.c (init and final xor 0xFFFFFFFF): 8 bytes per instruction.

        default rel

        section .text

        global  crc32_asm
crc32_asm:
        mov     eax, 0xFFFFFFFF
.loop8:
        cmp     rsi, 8
        jb      .tail
        crc32   rax, qword [rdi]
        add     rdi, 8
        sub     rsi, 8
        jmp     .loop8
.tail:
        test    rsi, rsi
        jz      .done
        crc32   eax, byte [rdi]
        inc     rdi
        dec     rsi
        jmp     .tail
.done:
        not     eax
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
