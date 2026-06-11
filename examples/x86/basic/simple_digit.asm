; ==========================================
; Simple Number Output (x86 32-bit)
; ==========================================
; Output the digit 5 to console
; Uses 32-bit registers

.data
result_msg: DB 'simple_digit: loaded 5, converted to ASCII, printed digit 5', 10, 0

.text
.org 0x100

_start:
    LOAD_IMM EAX, 5      ; Load the number 5
    LOAD_IMM EBX, 48     ; Load ASCII '0' value
    ADD EAX, EBX         ; Convert to ASCII ('0' + 5 = '5')

    LOAD_IMM EAX, result_msg
    OUTSTR EAX, 1

    HALT
