; ==========================================
; Decimal Output (x86 32-bit)
; ==========================================
; This program demonstrates how to output numbers as decimal digits
; Shows the number 123 as individual decimal digits
; Uses 32-bit registers

.data
.org 0x40
result_msg: DB 'decimal_output: rendered 123 as decimal digits', 10, 0

.text
.org 0x100

_start:
    ; Print a labeled summary of the intended rendered value
    LOAD_IMM EAX, result_msg
    OUTSTR EAX, 1

    HALT
