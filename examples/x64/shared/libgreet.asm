; ============================================================
; libgreet.asm — DASM Shared Library Example
; ============================================================
; Build: demi -A libgreet.asm --assembly-target x86-elf64 -o libgreet.so --shared
; Exports: greet — prints "Hello from shared library!" to stdout
; ============================================================

.section .text
.global greet
greet:
    ; write(1, msg, 27)
    mov eax, 1          ; sys_write
    mov edi, 1          ; stdout
    mov rsi, msg        ; address of message (relocation)
    mov edx, 27         ; message length
    syscall
    ret

.section rodata
msg:
    .string "Hello from shared library!\n"
