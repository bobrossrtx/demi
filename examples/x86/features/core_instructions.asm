; ==========================================
; Core Instructions Demo (x86 32-bit)
; ==========================================
; This program demonstrates the basic instructions that are currently
; implemented in the Demi Engine assembler
; Uses 32-bit registers

.text

_start:
    ; Basic immediate loading
    LOAD_IMM EAX, 100
    LOAD_IMM EBX, 50
    LOAD_IMM ECX, 25

    ; Arithmetic operations
    ADD EAX, EBX            ; EAX = 100 + 50 = 150
    SUB EBX, ECX            ; EBX = 50 - 25 = 25
    MUL ECX, ECX            ; ECX = 25 * 25 = 625

    ; Data movement
    MOV EDX, EAX            ; Copy EAX to EDX
    MOV ESI, EBX            ; Copy EBX to ESI
    MOV EDI, ECX            ; Copy ECX to EDI

    ; Comparison (sets flags)
    CMP EAX, EBX            ; Compare 150 vs 25

    ; Logical operations (using R8/R9 instead of ESP/EBP)
    LOAD_IMM R8, 0xFF
    LOAD_IMM R9, 0x0F
    AND R8, R9              ; R8 = 0xFF & 0x0F = 0x0F

    OR R8, EAX              ; R8 = bitwise OR
    XOR R9, EBX             ; R9 = bitwise XOR

    ; Stack operations
    PUSH EAX                ; Push 150 onto stack
    PUSH EBX                ; Push 25 onto stack
    PUSH ECX                ; Push 625 onto stack

    ; Pop in reverse order
    POP EDX                 ; EDX = 625
    POP ESI                 ; ESI = 25
    POP EDI                 ; EDI = 150

    ; Increment/Decrement
    INC EAX                 ; EAX = 151
    DEC EBX                 ; EBX = 24

    ; Bitwise NOT
    NOT R9                  ; R9 = ~R9

    ; End program
    HALT
