; ==========================================
; Indirect Addressing Demo (x64 64-bit)
; ==========================================
; Test LOADR instruction - demonstrates indirect addressing
; Uses 64-bit registers

.text

_start:
    ; Store value 42 at memory address 100
    LOAD_IMM RAX, 42
    LOAD_IMM RBX, 100    ; Load address into register
    STORE RAX, [RBX]     ; Store using bracket notation

    ; Store address 100 in register RCX
    LOAD_IMM RCX, 100

    ; Use LOADR to load from address stored in RCX
    LOADR RDX, RCX

    ; RDX should now contain 42
    LOAD_IMM RAX, 79
    OUT RAX, 1
    LOAD_IMM RAX, 75
    OUT RAX, 1
    LOAD_IMM RAX, 10
    OUT RAX, 1
    HALT
