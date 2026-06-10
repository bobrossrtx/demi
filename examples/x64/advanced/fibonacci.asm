; ==========================================
; Fibonacci Sequence Generator (x64 64-bit)
; ==========================================
; Calculates the first 10 Fibonacci numbers
; and stores them in memory starting at 0x500
; Uses 64-bit general purpose registers

.data
    header_msg: DB "Fibonacci (10 terms): ", 0
    header_msg_len EQU 22
    space: DB ' '
    newline: DB 10

    num_buf: RESB 32

.text

_start:
    ; Initialize registers
    LOAD_IMM RAX, 0      ; First number (F0)
    LOAD_IMM RBX, 1      ; Second number (F1)
    LOAD_IMM RCX, 10     ; Counter (calculate 10 numbers)
    LOAD_IMM RDX, 0x500  ; Memory pointer (keep clear of code/data)

    ; Store first two numbers
    STORER RDX, RAX      ; Store F0 (byte)
    INC RDX              ; Increment pointer

    STORER RDX, RBX      ; Store F1 (byte)
    INC RDX              ; Increment pointer
    
    LOAD_IMM RSI, 2      ; Load 2 for subtraction
    SUB RCX, RSI         ; Decrement counter by 2

loop:
    ; Calculate next number: F(n) = F(n-1) + F(n-2)
    ; RAX holds F(n-2), RBX holds F(n-1)
    
    MOV RSI, RBX         ; Save F(n-1) to temp
    ADD RBX, RAX         ; RBX = F(n-1) + F(n-2) = F(n)
    MOV RAX, RSI         ; RAX = old F(n-1) = new F(n-2)
    
    ; Store result
    STORER RDX, RBX      ; Store F(n) (byte)
    INC RDX
    
    ; Loop control
    DEC RCX
    JNZ loop

    ; Print the buffer as decimal numbers
    LOAD_IMM RCX, header_msg
    LOAD_IMM RDX, header_msg_len
    CALL print_str

    LOAD_IMM RDX, 0x500
    LOAD_IMM RCX, 10

print_output_loop:
    LOADR RAX, RDX
    PUSH RCX
    CALL print_u64
    POP RCX

    INC RDX
    DEC RCX
    JZ print_done

    PUSH RCX
    CALL print_space
    POP RCX
    JMP print_output_loop

print_done:
    CALL print_newline

    HALT

; Memory at 0x200+ contains: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34

; ==========================================
; Output helpers (Linux syscalls via INT 0x80)
; ==========================================

print_str:
    ; RCX = pointer, RDX = length
    LOAD_IMM RAX, 4      ; sys_write
    LOAD_IMM RBX, 1      ; fd = stdout
    INT 0x80
    RET

print_space:
    PUSH RDX
    LOAD_IMM RCX, space
    LOAD_IMM RDX, 1
    CALL print_str
    POP RDX
    RET

print_newline:
    PUSH RDX
    LOAD_IMM RCX, newline
    LOAD_IMM RDX, 1
    CALL print_str
    POP RDX
    RET

print_u64:
    ; Input: RAX = value
    PUSH RDX               ; Preserve caller's RDX (used as Fibonacci output pointer)
    LOAD_IMM RDI, num_buf
    CALL itoa

    LOAD_IMM RAX, 4      ; sys_write
    LOAD_IMM RBX, 1      ; stdout
    LOAD_IMM RCX, num_buf
    MOV RDX, RSI         ; length
    INT 0x80
    POP RDX
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
