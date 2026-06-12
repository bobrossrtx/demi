; Programming from the Ground Up — Chapter 4
; power.s: Recursive power function

.section .data
result: .dd 0

.section .text
.global _start

power:
    PUSH EBP
    MOV EBP, ESP

    MOV EAX, [EBP+12]       ; exp
    CMP EAX, 0
    JNE power_continue
    MOV EAX, 1
    JMP power_end

power_continue:
    MOV EAX, [EBP+12]       ; exp
    DEC EAX
    PUSH EAX

    MOV EAX, [EBP+8]        ; base
    PUSH EAX
    CALL power
    ADD ESP, 8

    MOV ECX, [EBP+8]        ; base
    IMUL EAX, ECX           ; power(base, exp-1) * base

power_end:
    MOV ESP, EBP
    POP EBP
    RET

_start:
    MOV EAX, 5
    PUSH EAX
    MOV EAX, 2
    PUSH EAX
    CALL power
    ADD ESP, 8

    MOV [result], EAX
    MOV EAX, 1
    MOV EBX, [result]
    INT 0x80
