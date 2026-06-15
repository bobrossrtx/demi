; prog.asm — DASM program importing libimport.so
; Prints before/after calling shared library compute()

.section rodata
before_msg:
    DB 66, 101, 102, 111, 114, 101, 32, 108   ; "Before l"
    DB 105, 98, 32, 99, 97, 108, 108, 46      ; "ib call."
    DB 46, 46, 10, 0                           ; "..\n\0"
after_msg:
    DB 108, 105, 98, 32, 99, 97, 108, 108      ; "lib call"
    DB 32, 100, 111, 110, 101, 33, 10, 0       ; " done!\n\0"

.section .text
.global main
.extern compute

main:
    ; Print "Before lib call...\n"
    mov eax, 1
    mov edi, 1
    mov rsi, before_msg
    mov edx, 19              ; "Before lib call...\n" (19 chars)
    syscall

    ; Call compute(10, 20) from shared library
    mov edi, 10
    mov esi, 20
    call compute

    ; Print "lib call done!\n"
    mov eax, 1
    mov edi, 1
    mov rsi, after_msg
    mov edx, 15              ; "lib call done!\n" (15 chars)
    syscall

    mov eax, 0
    ret
