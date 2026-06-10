; ==========================================
; Simple Addition Program (x86 32-bit)
; ==========================================
; This program adds two numbers and demonstrates basic arithmetic
; Uses 32-bit registers (EAX, EBX, ECX, EDX)

.text

_start:
    ; Load first number (42) into EAX
    LOAD_IMM EAX, 42
    
    ; Load second number (13) into EBX  
    LOAD_IMM EBX, 13
    
    ; Add EAX + EBX, result goes into EAX
    ADD EAX, EBX
    
    ; Copy result to other registers for demonstration
    MOV ECX, EAX             ; Copy result to ECX
    MOV EDX, EAX             ; Copy result to EDX
    
    ; Perform additional operations on the result
    LOAD_IMM ESI, 10
    SUB ECX, ESI             ; ECX = result - 10 = 55 - 10 = 45
    
    ADD EDX, ESI             ; EDX = result + 10 = 55 + 10 = 65
    
    ; End program
    ; Print "OK"
    LOAD_IMM EAX, 79
    OUT EAX, 1
    LOAD_IMM EAX, 75
    OUT EAX, 1
    LOAD_IMM EAX, 10
    OUT EAX, 1
    HALT

; Results:
; EAX = 55 (main result)
; ECX = 45 (result - 10)
; EDX = 65 (result + 10)
