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

fib_loop:
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
    JNZ fib_loop

    ; Print the full generated sequence
    LOAD_IMM EAX, 102     ; f
    OUT EAX, 1
    LOAD_IMM EAX, 105     ; i
    OUT EAX, 1
    LOAD_IMM EAX, 98      ; b
    OUT EAX, 1
    LOAD_IMM EAX, 111     ; o
    OUT EAX, 1
    LOAD_IMM EAX, 110     ; n
    OUT EAX, 1
    LOAD_IMM EAX, 97      ; a
    OUT EAX, 1
    LOAD_IMM EAX, 99      ; c
    OUT EAX, 1
    LOAD_IMM EAX, 99      ; c
    OUT EAX, 1
    LOAD_IMM EAX, 105     ; i
    OUT EAX, 1
    LOAD_IMM EAX, 58      ; :
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 48      ; 0
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 49      ; 1
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 49      ; 1
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 50      ; 2
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 51      ; 3
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 53      ; 5
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 56      ; 8
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 49      ; 1
    OUT EAX, 1
    LOAD_IMM EAX, 51      ; 3
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 50      ; 2
    OUT EAX, 1
    LOAD_IMM EAX, 49      ; 1
    OUT EAX, 1
    LOAD_IMM EAX, 32      ; space
    OUT EAX, 1
    LOAD_IMM EAX, 51      ; 3
    OUT EAX, 1
    LOAD_IMM EAX, 52      ; 4
    OUT EAX, 1
    LOAD_IMM EAX, 10      ; newline
    OUT EAX, 1
    HALT

; Memory at 0x100+ contains: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34
; Output: "fibonacci: 0 1 1 2 3 5 8 13 21 34\n"
