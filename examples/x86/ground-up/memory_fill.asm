; Memory Fill using REP STOSB
; Fills buffer with repeated byte

.section .data
buf:    .zero 32

.section .text
.global _start
_start:
    MOV EDI, buf
    MOV AL, 42
    MOV ECX, 30
    CLD
    REP
    STOSB

    MOV byte [EDI], 0x0A
    INC EDI
    MOV byte [EDI], 0

    MOV EAX, 4
    MOV EBX, 1
    MOV ECX, buf
    MOV EDX, 32
    INT 0x80

    MOV EAX, 1
    MOV EBX, 0
    INT 0x80
