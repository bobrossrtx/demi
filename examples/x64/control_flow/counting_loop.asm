; ==========================================
; Counting Loop Example (x64 64-bit)
; ==========================================
; Demonstrates a counting loop from 1 to 5
; Uses 64-bit registers
;
; Expected Output:
;   1 2 3 4 5

.text
.org 0x80

_start:
    ; Initialize counter in RAX
    LOAD_IMM RAX, 1     ; Start from 1
    LOAD_IMM RBX, 5     ; Loop limit
    
loop_start:
    ; Output current counter value as ASCII character
    ; Add 48 to convert number to ASCII digit
    MOV RCX, RAX        ; Copy counter to RCX
    LOAD_IMM RDX, 48    ; ASCII offset for '0'
    ADD RCX, RDX        ; Convert to ASCII digit
    OUT RCX, 1          ; Output to console
    
    ; Output a space
    LOAD_IMM RCX, 32    ; ASCII space
    OUT RCX, 1
    
    ; Increment counter
    INC RAX
    
    ; Check if we've reached the limit
    CMP RAX, RBX
    JLE loop_start      ; Jump if RAX <= RBX
    
    ; Output newline at end
    LOAD_IMM RAX, 10    ; ASCII newline
    OUT RAX, 1
    
    HALT
