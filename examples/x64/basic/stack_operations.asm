; ==========================================
; Stack Operations (x64 64-bit)
; ==========================================
; Demonstrates PUSH and POP operations
; Uses 64-bit stack pointer (RSP)
;
; Expected Output:
;   Stack test: PUSH 42,13,7 → POP = 7,13,42 (LIFO)

.data
.org 0x50
msg: DB "Stack test: PUSH 42,13,7 -> POP = 7,13,42 (LIFO)", 10, 0

.text
.org 0xA0

_start:
    ; Initialize some values
    LOAD_IMM RAX, 42
    LOAD_IMM RBX, 13
    LOAD_IMM RCX, 7
    
    ; Push values onto stack
    PUSH RAX            ; Push 42
    PUSH RBX            ; Push 13
    PUSH RCX            ; Push 7
    
    ; Clear registers
    LOAD_IMM RAX, 0
    LOAD_IMM RBX, 0
    LOAD_IMM RCX, 0
    
    ; Pop values back (in reverse order)
    POP RDX             ; RDX = 7
    POP RSI             ; RSI = 13
    POP RDI             ; RDI = 42
    
    ; Print descriptive message
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1
    
    HALT

; Stack demonstration:
; PUSH stores on stack, POP retrieves (LIFO order)
; x64 uses 8-byte stack alignment
