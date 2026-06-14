; ==========================================
; Core Instructions Demo (x64 64-bit)
; ==========================================
; Demonstrates all basic Demi VM instructions
; Uses 64-bit registers
;
; Expected Output:
;   Core instructions executed successfully

.data
.org 0x50
msg: DB "Core instructions executed successfully", 10, 0

.text
.org 0x90

_start:
    ; Run through all core instructions
    LOAD_IMM RAX, 100
    LOAD_IMM RBX, 50
    ADD RAX, RBX
    SUB RBX, RAX
    MUL RCX, RCX
    MOV RDX, RAX
    CMP RAX, RBX
    AND RAX, RBX
    OR RAX, RBX
    XOR RAX, RBX
    NOT RAX
    PUSH RAX
    POP RSI
    INC RAX
    DEC RBX

    ; Print result
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1
    HALT
