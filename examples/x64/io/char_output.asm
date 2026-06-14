; ==========================================
; Character Output Test (x64 64-bit)
; ==========================================
; Demonstrates single-character output using OUT instruction
; Uses 64-bit registers
;
; Expected Output:
;   Character output: H

.data
.org 0x50
label_msg: DB "Character output: "
label_end:

.text
.org 0x80

_start:
    ; Print label
    LOAD_IMM RAX, label_msg
    OUTSTR RAX, 1

    ; Output a single character via OUT
    LOAD_IMM RAX, 72     ; ASCII 'H'
    OUT RAX, 1

    ; Newline
    LOAD_IMM RAX, 10
    OUT RAX, 1

    HALT
