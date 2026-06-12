; Programming from the Ground Up — Chapter 5
; toupper.s: Convert stdin text to uppercase

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

    ; Convert in-place: each dword gets masked and checked
    MOV EDI, 0
convert_loop:
    CMP EDI, ESI
    JGE write_out

    ; Load dword, isolate each byte
    MOV EAX, [buffer+EDI]
    MOV ECX, EAX
    AND ECX, 0x000000FF ; byte 0
    CMP ECX, 97
    JL check_byte1
    CMP ECX, 122
    JG check_byte1
    SUB ECX, 32
    AND EAX, 0xFFFFFF00
    OR EAX, ECX

check_byte1:
    MOV ECX, EAX
    SHR ECX, 8
    AND ECX, 0x000000FF
    CMP ECX, 97
    JL check_byte2
    CMP ECX, 122
    JG check_byte2
    SUB ECX, 32
    AND EAX, 0xFFFF00FF
    SHL ECX, 8
    OR EAX, ECX

check_byte2:
    MOV ECX, EAX
    SHR ECX, 16
    AND ECX, 0x000000FF
    CMP ECX, 97
    JL check_byte3
    CMP ECX, 122
    JG check_byte3
    SUB ECX, 32
    AND EAX, 0xFF00FFFF
    SHL ECX, 16
    OR EAX, ECX

check_byte3:
    MOV ECX, EAX
    SHR ECX, 24
    AND ECX, 0x000000FF
    CMP ECX, 97
    JL store_back
    CMP ECX, 122
    JG store_back
    SUB ECX, 32
    AND EAX, 0x00FFFFFF
    SHL ECX, 24
    OR EAX, ECX

store_back:
    MOV [buffer+EDI], EAX
    ADD EDI, 4
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
