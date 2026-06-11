; ==========================================
; Labels and Strings Demo (x86 32-bit)
; ==========================================
; Demonstrates DB labels with LOAD_IMM for address referencing
; Uses 32-bit addressing with .data and .text sections

.data
    mystring: DB 'labels_and_strings: LOAD_IMM resolved this labeled string', 10, 0

.text
.org 0x100

_start:
    ; Load address of string and output it
    LOAD_IMM EAX, mystring  ; EAX = address of string data
    OUTSTR EAX, 1           ; Output string at [EAX] to console
    HALT
