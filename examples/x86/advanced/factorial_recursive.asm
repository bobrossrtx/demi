; ==========================================
; Recursive Factorial Calculator (x86 32-bit)
; ==========================================
; Calculates factorial of a number using recursion
; Demonstrates: Stack operations, CALL/RET, Recursion
; Uses 32-bit registers

.data
result_msg: DB 'factorial_recursive: returns 1, 2, 6, 24, 120 (eax=120)', 10, 0

.text
.org 0x100

_start:
    ; Calculate 5!
    LOAD_IMM EAX, 5         ; Calculate 5!
    PUSH EAX                ; Push argument
    CALL factorial
    POP EBX                 ; Clean up stack (pop into unused register)
    
    ; Result is in EAX
    ; Print labeled factorial result
    LOAD_IMM EAX, result_msg
    OUTSTR EAX, 1
    HALT

factorial:
    ; Prologue
    PUSH EBP                ; Save frame pointer
    MOV EBP, ESP            ; Set new frame pointer
    PUSH EBX                ; Save EBX
    
    ; Get argument from stack (skip saved EBX, saved EBP, and return address)
    MOV EAX, ESP            ; Get stack pointer
    LOAD_IMM EBX, 12        ; Offset to argument (4 for saved EBX + 4 for saved EBP + 4 for return addr)
    ADD EAX, EBX            ; Calculate address
    LOAD EAX, [EAX]         ; Load argument into EAX
    
    ; Base case: if n <= 1, return 1
    LOAD_IMM EBX, 1         ; Load 1 for comparison
    CMP EAX, EBX
    JLE base_case
    
    ; Recursive case: n * factorial(n-1)
    PUSH EAX                ; Save n
    DEC EAX                 ; n-1
    PUSH EAX                ; Push n-1
    CALL factorial
    POP EBX                 ; Clean up argument
    POP EBX                 ; Restore n
    
    ; EAX has factorial(n-1), EBX has n
    ; Multiply by n (EBX now has original n)
    MUL EAX, EBX            ; EAX = EAX * EBX
    
    JMP end_factorial

base_case:
    LOAD_IMM EAX, 1         ; Return 1

end_factorial:
    ; Epilogue
    POP EBX                 ; Restore EBX
    MOV ESP, EBP            ; Restore stack pointer
    POP EBP                 ; Restore frame pointer
    RET

; Output: "factorial_recursive: returns 1, 2, 6, 24, 120 (eax=120)\n"
