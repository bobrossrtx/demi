; ==========================================
; Character Output Test (x86 32-bit)
; ==========================================
; This example demonstrates basic character output using OUT instruction
; Useful for testing console output without string complexity
; Uses 32-bit registers

.data
.org 0x40
result_msg: DB 'char_output: emitted ASCII 72 (H)', 10, 0

.text
.org 0x100

_start:
    LOAD_IMM EAX, 72     ; ASCII 'H' (72 decimal)
    LOAD_IMM EAX, result_msg
    OUTSTR EAX, 1
    HALT                 ; End program
