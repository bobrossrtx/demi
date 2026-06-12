; Programming from the Ground Up — Chapter 5
; toupper.s: Convert stdin text to uppercase (now with byte ops!)

.section .bss
buffer: .resb 256

.section .text
.global _start
_start:

read_loop:
    MOV EAX, 3          ; sys_read
    MOV EBX, 0          ; stdin
    MOV ECX, buffer
    MOV EDX, 256
    INT 0x80
    CMP EAX, 0
    JLE exit
    MOV ESI, EAX        ; bytes read

    MOV EDI, 0
convert_loop:
    CMP EDI, ESI
    JGE write_out

    MOV AL, [buffer+EDI]    ; load byte
    CMP AL, 97              ; 'a'
    JB next_char
    CMP AL, 122             ; 'z'
    JA next_char
    SUB AL, 32              ; 'a' -> 'A'
    MOV [buffer+EDI], AL    ; store byte back

next_char:
    INC EDI
    JMP convert_loop

write_out:
    MOV EAX, 4
    MOV EBX, 1
    MOV ECX, buffer
    MOV EDX, ESI
    INT 0x80
    JMP read_loop

exit:
    MOV EAX, 1
    MOV EBX, 0
    INT 0x80
