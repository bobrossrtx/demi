; prog.asm — DASM program importing shared library

.section rodata
before_msg:
    DB "Before lib call...", 10, 0
after_msg:
    DB "lib call done!", 10, 0

.section .text
.global main
.extern say_hello
.extern compute

main:
    mov eax, 1
    mov edi, 1
    mov rsi, before_msg
    mov edx, 19
    syscall

    call say_hello

    mov edi, 10
    mov esi, 20
    call compute

    mov eax, 1
    mov edi, 1
    mov rsi, after_msg
    mov edx, 15
    syscall

    mov eax, 0
    ret
