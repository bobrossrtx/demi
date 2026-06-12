; Programming from the Ground Up — Chapter 4
; factorial.s: Recursive factorial function

.section .data
result: .dd 0

.section .text
.global _start

factorial:
    PUSH EBP
    MOV EBP, ESP

    MOV EAX, [EBP+8]        ; n
    CMP EAX, 1
    JG factorial_recurse
    MOV EAX, 1
    JMP factorial_end

factorial_recurse:
    MOV EAX, [EBP+8]        ; n
    DEC EAX
    PUSH EAX
    CALL factorial
    ADD ESP, 4

    MOV ECX, [EBP+8]        ; n
    IMUL EAX, ECX           ; factorial(n-1) * n

factorial_end:
    MOV ESP, EBP
    POP EBP
    RET

_start:
    MOV EAX, 5
    PUSH EAX
    CALL factorial
    ADD ESP, 4

    MOV [result], EAX
    MOV EAX, 1
    MOV EBX, [result]
    INT 0x80
