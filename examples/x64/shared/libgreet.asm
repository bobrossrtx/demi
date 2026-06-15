; libgreet.asm — DASM shared library example (working)
.section .text
.global greet
greet:
    mov eax, 42
    ret
