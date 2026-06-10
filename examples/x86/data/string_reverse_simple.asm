; Simple string reverse test
.data
.org 0x50
mystring: DB "Hello", 0

.text
.org 0x100

_start:
    LOAD_IMM EAX, 0x50
    ; Print the string
    OUTSTR EAX, 1
    LOAD_IMM EAX, 10
    OUT EAX, 1
    HALT
