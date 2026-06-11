; ==========================================
; Data Labels Demo (x86 32-bit)
; ==========================================
; This example demonstrates:
; - Multiple labeled data blocks
; - Different string lengths
; - Sequential data access
; Uses 32-bit addressing with 32-bit immediates

.org 0x100

; Define multiple labeled strings
greeting: 
    DB 'data_labels: greeting label says Hi!', 10, 0
message: 
    DB 'data_labels: second label says Labels work!', 10, 0

_start:
    ; Use both labels
    LOAD_IMM EAX, greeting
    OUTSTR EAX, 1

    LOAD_IMM EAX, message  
    OUTSTR EAX, 1

    HALT
