; Conditional Move Example
; Uses CMOVcc to avoid branches

.section .text
.global _start
_start:
    MOV EAX, 42
    MOV EBX, 17
    CMP EAX, EBX
    CMOVL EAX, EBX

    MOV ECX, 5
    MOV EDX, 10
    CMP ECX, EDX
    CMOVL ECX, EDX

    MOV EAX, 1
    MOV EBX, 0
    INT 0x80
