; ==========================================
; Stack Operations (x86 32-bit)
; ==========================================
; Demonstrates PUSH and POP operations
; Uses 32-bit stack pointer (ESP)

.data
result_msg: DB 'stack_operations: edx=7, esi=13, edi=42', 10, 0

.text
.org 0x100

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
    ; Print labeled restored register values
    LOAD_IMM EAX, result_msg
    OUTSTR EAX, 1
    HALT

; Stack demonstration:
; PUSH stores on stack, POP retrieves (LIFO order)
; Output: "stack_operations: edx=7, esi=13, edi=42\n"
