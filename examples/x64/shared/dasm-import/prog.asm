; prog.asm — DASM importing shared library
.section rodata
before_msg:
    DB 66, 101, 102, 111, 114, 101, 32, 108
    DB 105, 98, 32, 99, 97, 108, 108, 46
    DB 46, 46, 10, 0
after_msg:
    DB 108, 105, 98, 32, 99, 97, 108, 108
    DB 32, 100, 111, 110, 101, 33, 10, 0

.section .text
.global main
.extern say_hello
.extern compute

main:
    ; Print "Before lib call...\n"
    mov eax, 1
    mov edi, 1
    mov rsi, before_msg
    mov edx, 19
    syscall

    ; Call say_hello() from shared library — prints from .so
    call say_hello

    ; Call compute(10, 20)
    mov edi, 10
    mov esi, 20
    call compute

    ; Print "lib call done!\n"
    mov eax, 1
    mov edi, 1
    mov rsi, after_msg
    mov edx, 15
    syscall

    mov eax, 0
    ret
