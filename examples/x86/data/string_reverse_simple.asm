; Simple string reverse test
.data
.org 0x20
prefix_msg: DB "string_reverse_simple: ", 0

.org 0x50
mystring: DB "Hello", 0

.text
.org 0x100

_start:
    LOAD_IMM EAX, 0x50
    MOV EBX, EAX
    LOAD_IMM ESI, 0

find_end:
    LOADR ECX, EBX
    CMP ECX, ESI
    JZ found_end
    INC EBX
    JMP find_end

found_end:
    DEC EBX

reverse_loop:
    CMP EAX, EBX
    JGE print_result

    LOADR ECX, EAX
    LOADR EDX, EBX
    STORER EAX, EDX
    STORER EBX, ECX
    INC EAX
    DEC EBX
    JMP reverse_loop

print_result:
    LOAD_IMM EAX, 0x20
    OUTSTR EAX, 1

    LOAD_IMM EAX, 0x50
    OUTSTR EAX, 1
    LOAD_IMM EAX, 10
    OUT EAX, 1
    HALT
