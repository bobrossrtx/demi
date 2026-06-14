; ==========================================
; Simple Number Output (x64 64-bit)
; ==========================================
; Output the digit 5 to console
; Demonstrates ASCII conversion: digit + '0' = ASCII char
; Uses 64-bit registers
;
; Expected Output:
;   Number output: 5

.data
.org 0x50
msg: DB "Number output: "
msg_end:

.text
.org 0x80

_start:
    ; Print label
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1

    ; Convert and output the digit
    LOAD_IMM RAX, 5
    LOAD_IMM RBX, 48
    ADD RAX, RBX
    OUT RAX, 1

    ; Newline
    LOAD_IMM RAX, 10
    OUT RAX, 1

    HALT
