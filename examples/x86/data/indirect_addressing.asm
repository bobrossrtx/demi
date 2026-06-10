; ==========================================
; Indirect Addressing Demo (x86 32-bit)
; ==========================================
; Test LOADR instruction - demonstrates indirect addressing
; Uses 32-bit registers

.text

_start:
    ; Store value 42 at memory address 100
    LOAD_IMM EAX, 42
    STORE EAX, 100

    ; Store address 100 in register ECX
    LOAD_IMM ECX, 100

    ; Use LOADR to load from address stored in ECX
    LOADR EDX, ECX

    ; EDX should now contain 42
    ; Print "OK"
    LOAD_IMM EAX, 79
    OUT EAX, 1
    LOAD_IMM EAX, 75
    OUT EAX, 1
    LOAD_IMM EAX, 10
    OUT EAX, 1
    HALT
