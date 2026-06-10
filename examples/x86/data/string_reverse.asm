; String Reverse (x86 32-bit)
; Reverses a string in place using indirect addressing

.data
.org 0x50
string_data:
    DB "Hello World!", 0

.text
.org 0xF0

_start:
    ; Find string length
    LEA EAX, string_data
    MOV EBX, EAX
    LOAD_IMM ESI, 0          ; Load 0 for comparison

find_end:
    LOADR ECX, EBX
    CMP ECX, ESI             ; Compare with 0
    JZ found_end
    INC EBX
    JMP find_end

found_end:
    DEC EBX

reverse_loop:
    CMP EAX, EBX
    JGE done
    
    LOADR ECX, EAX      ; Load character from start
    LOADR EDX, EBX      ; Load character from end
    
    ; Swap using STORE (direct addressing)
    STORE EDX, [EAX]    ; Store end char at start position
    STORE ECX, [EBX]    ; Store start char at end position
    
    INC EAX
    DEC EBX
    JMP reverse_loop

done:
    ; Print reversed string
    LOAD_IMM EAX, 0x50
    OUTSTR EAX, 1
    LOAD_IMM EAX, 10
    OUT EAX, 1
    HALT
