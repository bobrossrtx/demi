; String Reverse (x64 64-bit)
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
    CMP RAX, RBX
    JGE done
    
    LOADR RCX, RAX      ; Load character from start
    LOADR RDX, RBX      ; Load character from end
    
    ; Swap using STORE (direct addressing)
    STORE RDX, [RAX]    ; Store end char at start position
    STORE RCX, [RBX]    ; Store start char at end position
    
    INC EAX
    DEC EBX
    JMP reverse_loop

done:
    LOAD_IMM RAX, 0x50
    OUTSTR RAX, 1
    LOAD_IMM RAX, 10
    OUT RAX, 1
    HALT
