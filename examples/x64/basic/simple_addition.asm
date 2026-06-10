; ==========================================
; Simple Addition Program (x64 64-bit)
; ==========================================
; This program adds two numbers and demonstrates basic arithmetic
; Uses 64-bit registers (RAX, RBX, RCX, RDX)

.text

_start:
    ; Load first number (42) into RAX
    LOAD_IMM RAX, 42
    
    ; Load second number (13) into RBX  
    LOAD_IMM RBX, 13
    
    ; Add RAX + RBX, result goes into RAX
    ADD RAX, RBX
    
    ; Copy result to other registers for demonstration
    MOV RCX, RAX             ; Copy result to RCX
    MOV RDX, RAX             ; Copy result to RDX
    
    ; Perform additional operations on the result
    LOAD_IMM RSI, 10
    SUB RCX, RSI             ; RCX = result - 10 = 55 - 10 = 45
    
    ADD RDX, RSI             ; RDX = result + 10 = 55 + 10 = 65
    
    ; End program
    LOAD_IMM RAX, 79
    OUT RAX, 1
    LOAD_IMM RAX, 75
    OUT RAX, 1
    LOAD_IMM RAX, 10
    OUT RAX, 1
    HALT

; Results:
; RAX = 55 (main result)
; RCX = 45 (result - 10)
; RDX = 65 (result + 10)
