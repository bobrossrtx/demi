; libimport.asm — shared library for DASM import demo
; Exports get_message() and compute(a,b)
.section .text
.global get_message
.global compute
get_message:
    mov eax, 42
    ret
compute:
    mov eax, edi
    add eax, esi
    ret
