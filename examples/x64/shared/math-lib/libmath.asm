; libmath.asm — Math shared library
; Exports: math_add, math_sub, math_mul

.section .text
.global math_add
math_add:
    mov eax, edi
    add eax, esi
    ret

.global math_sub
math_sub:
    mov eax, edi
    sub eax, esi
    ret

.global math_mul
math_mul:
    mov eax, edi
    imul eax, esi
    ret
