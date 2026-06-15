; prog.asm — DASM program importing libimport.so functions
; Demonstrates DASM calling DASM shared library
; Build:
;   demi -A prog.asm --assembly-target x86-elf64 -ao prog.o
;   gcc -no-pie -o prog prog.o -L. -l:libimport.so -Wl,-rpath,.

.section .text
.global main
.extern get_message
.extern compute
main:
    ; Call get_message() — returns 42
    call get_message
    ; Call compute(10, 20) — first arg in EDI, second in ESI
    mov edi, 10
    mov esi, 20
    call compute
    ; compute returns EAX = 30, get_message returns EAX = 42
    ; Return the sum: 42 + 30 = 72 (no, they overwrite each other)
    mov eax, 0
    ret
