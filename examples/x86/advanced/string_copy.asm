; String Copy using REP MOVSB
; Copies source string to destination buffer

.section .data
src:    .string "Hello from DASM string copy!"
        .zero 1
dst:    .zero 64

.section .text
.global _start
_start:
    MOV ESI, src
    MOV EDI, dst
    MOV ECX, 28
    CLD
    REP
    MOVSB

    MOV EAX, 4
    MOV EBX, 1
    MOV ECX, dst
    MOV EDX, 28
    INT 0x80

    MOV EAX, 1
    MOV EBX, 0
    INT 0x80
