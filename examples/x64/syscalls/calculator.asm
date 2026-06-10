; ==========================================
; Simple Calculator (x64 64-bit)
; ==========================================
; Interactive calculator that:
; 1. Prompts for first number
; 2. Prompts for operator (+, -, *, /)
; 3. Prompts for second number
; 4. Displays the result
;
; Demonstrates string-to-integer conversion (atoi) and integer-to-string conversion (itoa)

.data
    prompt1: DB "Enter first number: ", 0
    prompt1_len EQU 20
    prompt2: DB "Enter operator (+,-,*,/): ", 0
    prompt2_len EQU 26
    prompt3: DB "Enter second number: ", 0
    prompt3_len EQU 21
    result_msg: DB "Result: ", 0
    result_msg_len EQU 8
    newline: DB 10
    error_msg: DB "Error: Invalid input or division by zero", 10, 0
    error_msg_len EQU 41
    
    input_buffer: RESB 32   ; Buffer for number input
    operator_buffer: RESB 2  ; Buffer for operator
    result_buffer: RESB 32   ; Buffer for result string

.text
_start:

    
    ; === Get first number ===
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, prompt1
    LOAD_IMM RDX, prompt1_len
    INT 0x80
    
    ; Read first number
    LOAD_IMM RAX, 3
    LOAD_IMM RBX, 0
    LOAD_IMM RCX, input_buffer
    LOAD_IMM RDX, 32
    INT 0x80
    
    ; Convert string to integer (atoi)
    LOAD_IMM RSI, input_buffer
    CALL atoi
    MOV RDI, RAX            ; Store first number in RDI
    
    ; === Get operator ===
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, prompt2
    LOAD_IMM RDX, prompt2_len
    INT 0x80
    
    ; Read operator
    LOAD_IMM RAX, 3
    LOAD_IMM RBX, 0
    LOAD_IMM RCX, operator_buffer
    LOAD_IMM RDX, 2
    INT 0x80

    
    ; === Get second number ===
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, prompt3
    LOAD_IMM RDX, prompt3_len
    INT 0x80
    
    ; Read second number
    LOAD_IMM RAX, 3
    LOAD_IMM RBX, 0
    LOAD_IMM RCX, input_buffer
    LOAD_IMM RDX, 32
    INT 0x80
    
    ; Convert string to integer
    LOAD_IMM RSI, input_buffer
    CALL atoi
    MOV RSI, RAX            ; Store second number in RSI
    
    ; === Perform calculation ===
    MOV RAX, RDI            ; First number
    MOV RCX, RSI            ; Second number
    LOAD_IMM RDX, operator_buffer
    LOADR RBX, RDX           ; Operator (single byte)
    
    ; Check operator
    CMP RBX, 43             ; '+' ASCII
    JZ do_add
    CMP RBX, 45             ; '-' ASCII
    JZ do_sub
    CMP RBX, 42             ; '*' ASCII
    JZ do_mul
    CMP RBX, 47             ; '/' ASCII
    JZ do_div
    JMP error_exit
    
do_add:
    ADD RAX, RCX
    JMP display_result
    
do_sub:
    SUB RAX, RCX
    JMP display_result
    
do_mul:
    MUL RAX, RAX, RCX
    JMP display_result
    
do_div:
    CMP RCX, 0
    JZ error_exit           ; Division by zero
    LOAD_IMM RDX, 0         ; Clear RDX for division
    DIV RAX, RAX, RCX
    JMP display_result
    
display_result:
    ; RAX contains the result
    ; Convert integer to string
    LOAD_IMM RDI, result_buffer
    CALL itoa
    
    ; Print "Result: "
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, result_msg
    LOAD_IMM RDX, result_msg_len
    INT 0x80
    
    ; Print result number
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, result_buffer
    MOV RDX, RSI            ; Length from itoa
    INT 0x80
    
    ; Print newline
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, newline
    LOAD_IMM RDX, 1
    INT 0x80
    
    JMP exit_program

error_exit:
    ; Print error message
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, error_msg
    LOAD_IMM RDX, error_msg_len
    INT 0x80
    JMP exit_program

exit_program:
    LOAD_IMM RAX, 1
    LOAD_IMM RBX, 0
    INT 0x80
    HALT

; ==========================================
; Function: atoi (ASCII to Integer)
; Input: RSI = pointer to string
; Output: RAX = integer value
; Preserves: RDI
; Modifies: RAX, RBX, RCX, RDX, RSI
; ==========================================
atoi:
    LOAD_IMM RAX, 0         ; Result accumulator
    
atoi_loop:
    LOADR RBX, RSI          ; Load byte from address in RSI
    
    ; Check if end of string (newline or null)
    CMP RBX, 10             ; Newline
    JZ atoi_done
    CMP RBX, 0              ; Null terminator
    JZ atoi_done
    CMP RBX, 32             ; Space
    JZ atoi_done
    
    ; Check if valid digit (0-9)
    CMP RBX, 48             ; '0' ASCII
    JL atoi_done
    CMP RBX, 57             ; '9' ASCII
    JG atoi_done
    
    ; Convert ASCII to digit: digit = char - '0'
    LOAD_IMM RCX, 48
    SUB RBX, RCX
    
    ; result = result * 10 + digit
    LOAD_IMM RCX, 10
    MUL RAX, RAX, RCX            ; RAX *= 10
    ADD RAX, RBX            ; RAX += digit
    
    INC RSI
    JMP atoi_loop
    
atoi_done:
    RET

; ==========================================
; Function: itoa (Integer to ASCII)
; Input: RAX = integer value, RDI = destination buffer
; Output: RSI = length of string
; Modifies: RAX, RBX, RCX, RDX, RDI, RSI
; ==========================================
itoa:
    LOAD_IMM RSI, 0         ; Length counter
    LOAD_IMM RCX, 10        ; Base 10
    
    ; Handle zero special case
    CMP RAX, 0
    JNZ itoa_convert
    LOAD_IMM RBX, 48        ; '0'
    STORER RDI, RBX         ; Store at address in RDI
    LOAD_IMM RSI, 1
    RET
    
itoa_convert:
    ; Convert digits in reverse, then we'll reverse the string
    MOV RBX, RDI            ; Save start position
    
itoa_digit_loop:
    LOAD_IMM RDX, 0         ; Clear RDX for division
    DIV RAX, RAX, RCX            ; Divide by 10, quotient in RAX, remainder in RDX
    LOAD_IMM RCX, 48
    ADD RDX, RCX            ; Convert remainder to ASCII
    STORER RDI, RDX         ; Store at address in RDI
    INC RDI
    INC RSI
    
    CMP RAX, 0
    LOAD_IMM RCX, 10
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
    RET
