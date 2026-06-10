; ==========================================
; Recursive Factorial Calculator (x64 64-bit)
; ==========================================
; Calculates factorial of a number using recursion
; Demonstrates: Stack operations, CALL/RET, Recursion
; Uses 64-bit registers

.data
    prefix_msg: DB "factorial_recursive(", 0
    prefix_msg_len EQU 20
    mid_msg: DB ") = ", 0
    mid_msg_len EQU 4
    newline: DB 10
    num_buf: RESB 32

.text

_start:
    ; Print 5 factorials: 1! .. 5!
    LOAD_IMM RAX, 1         ; i = 1

print_loop:
    ; Keep i across printing and recursion (print_u64 clobbers RSI/RBP/etc)
    PUSH RAX                ; copy A: loop control
    PUSH RAX                ; copy B: printing/argument

    ; Print: factorial_recursive(
    LOAD_IMM RCX, prefix_msg
    LOAD_IMM RDX, prefix_msg_len
    CALL print_str

    ; Print i
    POP RAX                 ; pop copy B
    PUSH RAX                ; copy C: factorial argument
    CALL print_u64

    ; Print: ) =
    LOAD_IMM RCX, mid_msg
    LOAD_IMM RDX, mid_msg_len
    CALL print_str

    ; Compute factorial_recursive(i)
    POP RAX                 ; pop copy C
    PUSH RAX                ; push argument
    CALL factorial
    POP RCX                 ; clean up argument

    ; Print result
    CALL print_u64
    CALL print_newline

    ; Restore i and continue
    POP RAX                 ; pop copy A
    INC RAX
    LOAD_IMM RDX, 5
    CMP RAX, RDX
    JLE print_loop

    HALT

factorial:
    ; Get argument from stack frame created by CALL.
    ; CALL pushes [old FP][return addr] (8 bytes total, 4 bytes each),
    ; so the first user argument is at (SP + 8).
    MOV RAX, RSP            ; RAX = SP (points at return address)
    LOAD_IMM RDX, 8         ; Offset to first argument
    ADD RAX, RDX            ; RAX = SP + 8
    LOADR RAX, RAX          ; RAX = *(uint8_t*)RAX (byte-oriented)
    
    ; Base case: if n <= 1, return 1
    LOAD_IMM RDX, 1
    CMP RAX, RDX
    JLE base_case
    
    ; Recursive case: n * factorial(n-1)
    PUSH RAX                ; Save n across recursive call
    DEC RAX                 ; n-1
    PUSH RAX                ; Push n-1 as argument
    CALL factorial
    POP RCX                 ; Clean up argument
    POP RDX                 ; Restore saved n

    ; RAX has factorial(n-1); multiply by n
    MUL64 RAX, RAX, RDX     ; RAX = RAX * n
    RET

base_case:
    LOAD_IMM RAX, 1         ; Return 1
    RET

; ==========================================
; Output helpers (Linux syscalls via INT 0x80)
; ==========================================

print_str:
    ; RCX = pointer, RDX = length
    LOAD_IMM RAX, 4      ; sys_write
    LOAD_IMM RBX, 1      ; fd = stdout
    INT 0x80
    RET

print_newline:
    LOAD_IMM RCX, newline
    LOAD_IMM RDX, 1
    JMP print_str

print_u64:
    ; Input: RAX = value
    LOAD_IMM RDI, num_buf
    CALL itoa

    LOAD_IMM RAX, 4      ; sys_write
    LOAD_IMM RBX, 1      ; stdout
    LOAD_IMM RCX, num_buf
    MOV RDX, RSI         ; length
    INT 0x80
    RET

; ==========================================
; Function: itoa (Integer to ASCII)
; Input: RAX = integer value, RDI = destination buffer
; Output: RSI = length of string
; ==========================================
itoa:
    LOAD_IMM RSI, 0         ; Length counter
    LOAD_IMM RBP, 10        ; Base 10

    ; Handle zero special case
    CMP RAX, 0
    JNZ itoa_convert
    LOAD_IMM RBX, 48        ; '0'
    STORER RDI, RBX         ; Store at address in RDI
    LOAD_IMM RSI, 1
    RET

itoa_convert:
    ; Convert digits in reverse, then reverse in place
    MOV RBX, RDI            ; Save start position

itoa_digit_loop:
    DIV RAX, RAX, RBP       ; Quotient in RAX, remainder in RDX
    LOAD_IMM RCX, 48
    ADD RDX, RCX            ; Convert remainder to ASCII
    STORER RDI, RDX         ; Store digit
    INC RDI
    INC RSI

    CMP RAX, 0
    JNZ itoa_digit_loop

    ; Reverse the string
    MOV RCX, RBX            ; Start position
    DEC RDI                 ; End position

itoa_reverse_loop:
    CMP RCX, RDI
    JGE itoa_done

    LOADR RAX, RCX          ; Load from start
    LOADR RBX, RDI          ; Load from end
    STORER RCX, RBX         ; Store at start
    STORER RDI, RAX         ; Store at end

    INC RCX
    DEC RDI
    JMP itoa_reverse_loop

itoa_done:
    LOAD_IMM RAX, 79
    OUT RAX, 1
    LOAD_IMM RAX, 75
    OUT RAX, 1
    LOAD_IMM RAX, 10
    OUT RAX, 1
    RET
