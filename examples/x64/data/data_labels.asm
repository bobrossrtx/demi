; ==========================================
; Data Labels Demo (x64 64-bit)
; ==========================================
; This example demonstrates:
; - Multiple labeled data blocks
; - Different string lengths
; - Sequential data access
; Uses 64-bit addressing with 32-bit immediates for E-registers

.org 0x100

; Define multiple labeled strings
greeting: 
    DB 'Hi!', 10, 0
message: 
    DB 'Labels work!', 10, 0

_start:
    ; Use both labels
    LOAD_IMM RAX, greeting
    OUTSTR RAX, 1

    LOAD_IMM RAX, message  
    OUTSTR RAX, 1

    HALT
