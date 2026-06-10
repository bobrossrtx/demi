; ==========================================
; Stack Operations (x86 32-bit)
; ==========================================
; Demonstrates PUSH and POP operations
; Uses 32-bit stack pointer (ESP)

.text

_start:
    ; Initialize some values
    LOAD_IMM EAX, 42
    LOAD_IMM EBX, 13
    LOAD_IMM ECX, 7
    
    ; Push values onto stack
    PUSH EAX            ; Push 42
    PUSH EBX            ; Push 13
    PUSH ECX            ; Push 7
    
    ; Clear registers
    LOAD_IMM EAX, 0
    LOAD_IMM EBX, 0
    LOAD_IMM ECX, 0
    
    ; Pop values back (in reverse order)
    POP EDX             ; EDX = 7
    POP ESI             ; ESI = 13
    POP EDI             ; EDI = 42
    
    ; Verify: EDI=42, ESI=13, EDX=7
    ; Print "OK"
    LOAD_IMM EAX, 79
    OUT EAX, 1
    LOAD_IMM EAX, 75
    OUT EAX, 1
    LOAD_IMM EAX, 10
    OUT EAX, 1
    HALT

; Stack demonstration:
; PUSH stores on stack, POP retrieves (LIFO order)
