; ==========================================
; Data Storage and String Output (x64 64-bit)
; ==========================================
; Demonstrates DB directive and string operations
; Uses 64-bit addressing
;
; Expected Output:
;   Hello, DemiEngine!
;   x64 64-bit mode

.data
.org 0x50
msg1: DB 'Hello, DemiEngine!', 10, 0

.org 0x80
msg2: DB 'x64 64-bit mode', 10, 0

.text
.org 0xA0

_start:
    ; Print first string
    LOAD_IMM RAX, msg1
    OUTSTR RAX, 1

    ; Print second string
    LOAD_IMM RAX, msg2
    OUTSTR RAX, 1

    HALT
