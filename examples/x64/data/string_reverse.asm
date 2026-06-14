; ==========================================
; String Reverse (x64 64-bit)
; ==========================================
; Reverses a string in place using indirect addressing
;
; Expected Output:
;   !dlroW olleH

.data
.org 0x50
string_data:
    DB "Hello World!", 0

.org 0x70
done_msg: DB "Reversed: ", 0

.text
.org 0xA0

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
    DEC EBX                  ; EBX points to last char

reverse_loop:
    CMP EAX, EBX
    JGE done
    
    LOADR RCX, EAX           ; Load character from start
    LOADR RDX, EBX           ; Load character from end
    
    ; Swap using STORER (indirect addressing)
    STORER EAX, RDX          ; Store end char at start position
    STORER EBX, RCX          ; Store start char at end position
    
    INC EAX
    DEC EBX
    JMP reverse_loop

done:
    ; Print label
    LOAD_IMM RAX, done_msg
    OUTSTR RAX, 1
    
    ; Print reversed string
    LOAD_IMM RAX, string_data
    OUTSTR RAX, 1
    
    ; Newline
    LOAD_IMM RAX, 10
    OUT RAX, 1
    HALT
