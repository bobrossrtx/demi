; ==========================================
; Data Storage and String Output (x86 32-bit)
; ==========================================
; Demonstrates DB directive and string operations
; Uses 32-bit addressing

; Store string data at address 0x50
.org 0x50
DB 'data_storage[0x50]', 10, 0

; Store another string at 0x70
.org 0x70
DB 'data_storage[0x70]', 10, 0

; Start program at address 0x90
.org 0x90

_start:
    ; Print first string
    LOAD_IMM EAX, 0x50      ; EAX = address of first string
    OUTSTR EAX, 1           ; Output string at [EAX]

    ; Print second string
    LOAD_IMM EAX, 0x70      ; EAX = address of second string
    OUTSTR EAX, 1           ; Output string at [EAX]

    HALT
