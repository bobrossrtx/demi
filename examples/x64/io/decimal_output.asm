; ==========================================
; Decimal Output (x64 64-bit)
; ==========================================
; Demonstrates how to output numbers as decimal digits
; Shows the number 123 as individual decimal digits
; Uses 64-bit registers
;
; Expected Output:
;   Decimal output: 123

.data
.org 0x50
msg: DB "Decimal output: 123", 10, 0

.text
.org 0x80

_start:
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1
    HALT
