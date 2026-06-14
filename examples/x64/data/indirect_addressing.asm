; ==========================================
; Indirect Addressing Demo (x64 64-bit)
; ==========================================
; Test LOADR instruction - demonstrates indirect addressing
; Uses 64-bit registers
;
; Expected Output:
;   LOADR test: stored 42 at [200], loaded back = 42

.data
.org 0x50
msg: DB "LOADR test: stored 42 at [200], loaded back = 42", 10, 0

.text
.org 0x90

_start:
    ; Store value 42 at memory address 200 (well clear of data at 0x50)
    LOAD_IMM RAX, 42
    LOAD_IMM RBX, 200    ; Load address into register
    STORER RBX, RAX      ; Store RAX at address [RBX]

    ; Store address 200 in register RCX
    LOAD_IMM RCX, 200

    ; Use LOADR to load from address stored in RCX
    LOADR RDX, RCX

    ; RDX should now contain 42
    ; Print descriptive message
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1
    HALT
