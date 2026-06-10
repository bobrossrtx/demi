; ==========================================
; Core Instructions Demo (x64 64-bit)
; ==========================================
; This program demonstrates the basic instructions that are currently
; implemented in the Demi Engine assembler
; Uses 64-bit registers

.text

_start:
    ; Basic immediate loading
    LOAD_IMM RAX, 100
    LOAD_IMM RBX, 50
    LOAD_IMM RCX, 25

    ; Arithmetic operations
    ADD RAX, RBX            ; RAX = 100 + 50 = 150
    SUB RBX, RCX            ; RBX = 50 - 25 = 25
    MUL RCX, RCX            ; RCX = 25 * 25 = 625

    ; Data movement
    MOV RDX, RAX            ; Copy RAX to RDX
    MOV RSI, RBX            ; Copy RBX to RSI
    MOV RDI, RCX            ; Copy RCX to RDI

    ; Comparison (sets flags)
    CMP RAX, RBX            ; Compare 150 vs 25

    ; Logical operations
    LOAD_IMM RSP, 0xFF
    LOAD_IMM RBP, 0x0F
    AND RSP, RBP            ; RSP = 0xFF & 0x0F = 0x0F

    OR RSP, RAX             ; RSP = bitwise OR
    XOR RBP, RBX            ; RBP = bitwise XOR

    ; Stack operations
    PUSH RAX                ; Push 150 onto stack
    PUSH RBX                ; Push 25 onto stack
    PUSH RCX                ; Push 625 onto stack

    ; Pop in reverse order
    POP RDX                 ; RDX = 625
    POP RSI                 ; RSI = 25
    POP RDI                 ; RDI = 150

    ; Increment/Decrement
    INC RAX                 ; RAX = 151
    DEC RBX                 ; RBX = 24

    ; Bitwise NOT
    NOT RBP                 ; RBP = ~RBP

    ; End program
    LOAD_IMM RAX, 79
    OUT RAX, 1
    LOAD_IMM RAX, 75
    OUT RAX, 1
    LOAD_IMM RAX, 10
    OUT RAX, 1
    HALT
