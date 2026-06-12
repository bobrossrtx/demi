; toupper with macros
.include "macros.inc"

.section .bss
buffer: .resb 256

.section .text
.global _start
_start:

read_loop:
    sys_read 0, buffer, 256
    CMP EAX, 0
    JLE exit
    MOV ESI, EAX

    MOV EDI, 0
convert_loop:
    CMP EDI, ESI
    JGE write_out
    MOV AL, [buffer+EDI]
    CMP AL, 97
    JB next_char
    CMP AL, 122
    JA next_char
    SUB AL, 32
    MOV [buffer+EDI], AL
next_char:
    INC EDI
    JMP convert_loop

write_out:
    sys_write 1, buffer, ESI
    JMP read_loop

exit:
    sys_exit 0
