; libimport.asm — shared library for DASM import demo
; Build: demi -A libimport.asm --assembly-target x86-elf64 -o libimport.so --shared

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
