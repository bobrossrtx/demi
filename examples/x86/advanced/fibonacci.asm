; ==========================================
; Fibonacci Sequence Generator (x86 32-bit)
; ==========================================
; Calculates the first 10 Fibonacci numbers
; and stores them in memory starting at 0x100
; Uses 32-bit general purpose registers

.text
.org 0x00

_start:
    ; Initialize registers
    LOAD_IMM EAX, 0      ; First number (F0)
    LOAD_IMM EBX, 1      ; Second number (F1)
    LOAD_IMM ECX, 10     ; Counter (calculate 10 numbers)
    LOAD_IMM EDX, 0x100  ; Memory pointer

    ; Store first two numbers
    STORE EAX, [EDX]     ; Store F0
    INC EDX              ; Increment pointer
    STORE EBX, [EDX]     ; Store F1
    INC EDX              ; Increment pointer
    
    LOAD_IMM ESI, 2      ; Load 2 for subtraction
    SUB ECX, ESI         ; Decrement counter by 2

loop:
    ; Calculate next number: F(n) = F(n-1) + F(n-2)
    ; EAX holds F(n-2), EBX holds F(n-1)
    
    MOV ESI, EBX         ; Save F(n-1) to temp
    ADD EBX, EAX         ; EBX = F(n-1) + F(n-2) = F(n)
    MOV EAX, ESI         ; EAX = old F(n-1) = new F(n-2)
    
    ; Store result
    STORE EBX, [EDX]
    INC EDX
    
    ; Loop control
    DEC ECX
    JNZ loop

    ; Print "OK"
    LOAD_IMM EAX, 79
    OUT EAX, 1
    LOAD_IMM EAX, 75
    OUT EAX, 1
    LOAD_IMM EAX, 10
    OUT EAX, 1
    HALT

; Memory at 0x100+ contains: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34
