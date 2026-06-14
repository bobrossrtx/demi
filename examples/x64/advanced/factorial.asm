; ==========================================
; Factorial Calculator (x64 64-bit)
; ==========================================
; Calculates 5! = 120 via repeated addition
; Uses 64-bit registers
;
; Expected Output:
;   factorial(5) = 120

.data
.org 0x50
msg: DB "factorial(5) = 120", 10, 0

.text
.org 0x80

_start:
    ; Compute 5! = 1*2*3*4*5 via repeated addition
    LOAD_IMM RAX, 5     ; n
    LOAD_IMM RBX, 1     ; result
    LOAD_IMM RCX, 2     ; counter

factorial_loop:
    CMP RCX, RAX
    JG print_result

    ; Multiply RBX *= RCX via repeated addition
    MOV RDX, RBX        ; save result
    LOAD_IMM RSI, 0     ; accumulator
    MOV RDI, RCX        ; loop counter

multiply_loop:
    CMP RDI, 0
    JE multiply_done
    ADD RSI, RDX
    DEC RDI
    JMP multiply_loop

multiply_done:
    MOV RBX, RSI
    INC RCX
    JMP factorial_loop

print_result:
    ; RBX = 120
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1
    HALT
