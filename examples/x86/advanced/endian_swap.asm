; Endian Swap using BSWAP

.section .data
be_val: .dd 0x12345678
result: .dd 0

.section .text
.global _start
_start:
    MOV EAX, [be_val]
    BSWAP EAX
    MOV [result], EAX
    MOV EAX, 1
    MOV EBX, 0
    INT 0x80
