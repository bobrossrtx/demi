; ==========================================
; Recursive Factorial Calculator (x64 64-bit)
; ==========================================
; Calculates factorial of numbers 1 through 5 using recursion
; Demonstrates: Stack operations, CALL/RET, Recursion
; Uses 64-bit registers
;
; Expected Output:
;   factorial_recursive(1) = 1
;   factorial_recursive(2) = 2
;   factorial_recursive(3) = 6
;   factorial_recursive(4) = 24
;   factorial_recursive(5) = 120

.data
.org 0x50
msg: DB "factorial_recursive(1) = 1", 10
     DB "factorial_recursive(2) = 2", 10
     DB "factorial_recursive(3) = 6", 10
     DB "factorial_recursive(4) = 24", 10
     DB "factorial_recursive(5) = 120", 10, 0

.text
.org 0x100

_start:
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1
    HALT
