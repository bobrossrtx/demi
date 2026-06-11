; ==========================================
; Hello World Program (x86 32-bit)
; ==========================================
; Demonstrates string output using DB directive
; Uses 32-bit addressing

.data
; Store "Hello, World!\n" at address 0x50
.org 0x50
hello_msg: DB 'hello_world: Hello, World!', 10, 0

.text
; Start instructions at address 0x70
.org 0x70

_start:
    ; Print the labeled string using OUTSTR
    LOAD_IMM EAX, hello_msg      ; EAX = address of string
    OUTSTR EAX, 1                ; Output string at [EAX] to console

    HALT
