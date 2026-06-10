; ==========================================
; Simple Calculator (x86 32-bit)
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
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, prompt1
    LOAD_IMM EDX, prompt1_len
    INT 0x80
    
    ; Read first number
    LOAD_IMM EAX, 3
    LOAD_IMM EBX, 0
    LOAD_IMM ECX, input_buffer
    LOAD_IMM EDX, 32
    INT 0x80
    CMP EAX, 0
    JLE exit_program        ; EOF or error
    
    ; Convert string to integer (atoi)
    LOAD_IMM ESI, input_buffer
    CALL atoi
    MOV EDI, EAX            ; Store first number in EDI
    
    ; === Get operator ===
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, prompt2
    LOAD_IMM EDX, prompt2_len
    INT 0x80
    
    ; Read operator
    LOAD_IMM EAX, 3
    LOAD_IMM EBX, 0
    LOAD_IMM ECX, operator_buffer
    LOAD_IMM EDX, 2
    INT 0x80
    CMP EAX, 0
    JLE exit_program        ; EOF or error
    
    ; Load operator into BL
    LOAD EBX, operator_buffer
    
    ; === Get second number ===
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, prompt3
    LOAD_IMM EDX, prompt3_len
    INT 0x80
    
    ; Read second number
    LOAD_IMM EAX, 3
    LOAD_IMM EBX, 0
    LOAD_IMM ECX, input_buffer
    LOAD_IMM EDX, 32
    INT 0x80
    CMP EAX, 0
    JLE exit_program        ; EOF or error
    
    ; Convert string to integer
    LOAD_IMM ESI, input_buffer
    CALL atoi
    MOV ESI, EAX            ; Store second number in ESI
    
    ; === Perform calculation ===
    MOV EAX, EDI            ; First number
    MOV ECX, ESI            ; Second number
    LOAD EBX, operator_buffer  ; Operator
    
    ; Check operator
    CMP EBX, 43             ; '+' ASCII
    JZ do_add
    CMP EBX, 45             ; '-' ASCII
    JZ do_sub
    CMP EBX, 42             ; '*' ASCII
    JZ do_mul
    CMP EBX, 47             ; '/' ASCII
    JZ do_div
    JMP error_exit
    
do_add:
    ADD EAX, ECX
    JMP display_result
    
do_sub:
    SUB EAX, ECX
    JMP display_result
    
do_mul:
    MUL EAX, ECX
    JMP display_result
    
do_div:
    CMP ECX, 0
    JZ error_exit           ; Division by zero
    LOAD_IMM EDX, 0         ; Clear EDX for division
    DIV EAX, ECX
    JMP display_result
    
display_result:
    ; EAX contains the result
    ; Convert integer to string
    LOAD_IMM EDI, result_buffer
    CALL itoa
    
    ; Print "Result: "
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, result_msg
    LOAD_IMM EDX, result_msg_len
    INT 0x80
    
    ; Print result number
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, result_buffer
    MOV EDX, ESI            ; Length from itoa
    INT 0x80
    
    ; Print newline
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, newline
    LOAD_IMM EDX, 1
    INT 0x80
    
    JMP exit_program

error_exit:
    ; Print error message
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, error_msg
    LOAD_IMM EDX, error_msg_len
    INT 0x80
    JMP exit_program

exit_program:
    LOAD_IMM EAX, 1
    LOAD_IMM EBX, 0
    INT 0x80
    HALT

; ==========================================
; Function: atoi (ASCII to Integer)
; Input: ESI = pointer to string
; Output: EAX = integer value
; Preserves: EDI
; Modifies: EAX, EBX, ECX, EDX, ESI
; ==========================================
atoi:
    LOAD_IMM EAX, 0         ; Result accumulator
    
atoi_loop:
    LOADR EBX, ESI          ; Load byte from address in ESI
    
    ; Check if end of string (newline or null)
    CMP EBX, 10             ; Newline
    JZ atoi_done
    CMP EBX, 0              ; Null terminator
    JZ atoi_done
    CMP EBX, 32             ; Space
    JZ atoi_done
    
    ; Check if valid digit (0-9)
    CMP EBX, 48             ; '0' ASCII
    JL atoi_done
    CMP EBX, 57             ; '9' ASCII
    JG atoi_done
    
    ; Convert ASCII to digit: digit = char - '0'
    LOAD_IMM ECX, 48
    SUB EBX, ECX
    
    ; result = result * 10 + digit
    LOAD_IMM ECX, 10
    MUL EAX, ECX            ; EAX *= 10
    ADD EAX, EBX            ; EAX += digit
    
    INC ESI
    JMP atoi_loop
    
atoi_done:
    RET

; ==========================================
; Function: itoa (Integer to ASCII)
; Input: EAX = integer value, EDI = destination buffer
; Output: ESI = length of string
; Modifies: EAX, EBX, ECX, EDX, EDI, ESI
; ==========================================
itoa:
    LOAD_IMM ESI, 0         ; Length counter
    LOAD_IMM ECX, 10        ; Base 10
    
    ; Handle zero special case
    CMP EAX, 0
    JNZ itoa_convert
    LOAD_IMM EBX, 48        ; '0'
    STORER EDI, EBX         ; Store at address in EDI
    LOAD_IMM ESI, 1
    RET
    
itoa_convert:
    ; Convert digits in reverse, then we'll reverse the string
    MOV EBX, EDI            ; Save start position
    
itoa_digit_loop:
    LOAD_IMM EDX, 0         ; Clear EDX for division
    DIV EAX, ECX            ; Divide by 10, quotient in EAX, remainder in EDX
    LOAD_IMM ECX, 48
    ADD EDX, ECX            ; Convert remainder to ASCII
    STORER EDI, EDX         ; Store at address in EDI
    INC EDI
    INC ESI
    
    CMP EAX, 0
    LOAD_IMM ECX, 10
    JNZ itoa_digit_loop
    
    ; Reverse the string
    MOV ECX, EBX            ; Start position
    DEC EDI                 ; End position
    
itoa_reverse_loop:
    CMP ECX, EDI
    JGE itoa_done
    
    LOADR EAX, ECX          ; Load from start
    LOADR EBX, EDI          ; Load from end
    STORER ECX, EBX         ; Store at start
    STORER EDI, EAX         ; Store at end
    
    INC ECX
    DEC EDI
    JMP itoa_reverse_loop
    
itoa_done:
    RET
