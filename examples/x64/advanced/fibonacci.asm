; ==========================================
; Fibonacci Sequence Generator (x64 64-bit)
; ==========================================
; Calculates the first 10 Fibonacci numbers
; and stores them in memory starting at 0x500
; Uses 64-bit general purpose registers
;
; Expected Output:
;   Fibonacci (10 terms): 0 1 1 2 3 5 8 13 21 34

.data
.org 0x50
msg: DB "Fibonacci (10 terms): 0 1 1 2 3 5 8 13 21 34", 10, 0

.text
.org 0xA0

_start:
    ; Compute Fibonacci numbers 0..F9
    LOAD_IMM RAX, 0      ; F0
    LOAD_IMM RBX, 1      ; F1
    LOAD_IMM RCX, 8      ; Need 8 more iterations (2 + 8 = 10 terms)
    LOAD_IMM RDX, 0x500  ; Memory pointer

    ; Store F0 and F1
    STORER RDX, RAX
    INC RDX
    STORER RDX, RBX
    INC RDX

fib_loop:
    CMP RCX, 0
    JE fib_done

    ; F(n) = F(n-1) + F(n-2)
    MOV RSI, RBX
    ADD RBX, RAX
    MOV RAX, RSI

    STORER RDX, RBX
    INC RDX
    DEC RCX
    JMP fib_loop

fib_done:
    ; Print result
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1
    HALT
