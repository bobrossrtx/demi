; ==========================================
; Calculator (x86 32-bit)
; ==========================================
; Interactive single-digit calculator that supports +, -, *, and /.

.data
    prompt1: DB "Enter first digit (0-9): ", 0
    prompt1_len EQU 25
    prompt2: DB "Enter operator (+,-,*,/): ", 0
    prompt2_len EQU 26
    prompt3: DB "Enter second digit (0-9): ", 0
    prompt3_len EQU 26
    result_msg: DB "calculator result: ", 0
    result_msg_len EQU 19
    error_msg: DB "calculator: invalid input", 10, 0
    error_msg_len EQU 26
    newline: DB 10

    input_a: RESB 2
    input_b: RESB 2
    operator_buffer: RESB 2
    result_buffer: RESB 4

.text
_start:
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, prompt1
    LOAD_IMM EDX, prompt1_len
    INT 0x80

    LOAD_IMM EAX, 3
    LOAD_IMM EBX, 0
    LOAD_IMM ECX, input_a
    LOAD_IMM EDX, 2
    INT 0x80
    CMP EAX, 0
    JLE exit_program

    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, prompt2
    LOAD_IMM EDX, prompt2_len
    INT 0x80

    LOAD_IMM EAX, 3
    LOAD_IMM EBX, 0
    LOAD_IMM ECX, operator_buffer
    LOAD_IMM EDX, 2
    INT 0x80
    CMP EAX, 0
    JLE exit_program

    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, prompt3
    LOAD_IMM EDX, prompt3_len
    INT 0x80

    LOAD_IMM EAX, 3
    LOAD_IMM EBX, 0
    LOAD_IMM ECX, input_b
    LOAD_IMM EDX, 2
    INT 0x80
    CMP EAX, 0
    JLE exit_program

    LOAD_IMM ESI, input_a
    LOADR EAX, ESI
    LOAD_IMM ECX, 0xFF
    AND EAX, ECX
    LOAD_IMM ECX, 48
    SUB EAX, ECX
    MOV EDI, EAX

    LOAD_IMM ESI, input_b
    LOADR EAX, ESI
    LOAD_IMM ECX, 0xFF
    AND EAX, ECX
    LOAD_IMM ECX, 48
    SUB EAX, ECX
    MOV ESI, EAX

    LOAD_IMM EDX, operator_buffer
    LOADR EBX, EDX
    LOAD_IMM ECX, 0xFF
    AND EBX, ECX

    MOV EAX, EDI
    CMP EBX, 43
    JZ do_add
    CMP EBX, 45
    JZ do_sub
    CMP EBX, 42
    JZ do_mul
    CMP EBX, 47
    JZ do_div
    JMP error_exit

do_add:
    ADD EAX, ESI
    JMP print_result

do_sub:
    SUB EAX, ESI
    JMP print_result

do_mul:
    MOV ECX, ESI
    MUL EAX, ECX
    JMP print_result

do_div:
    CMP ESI, 0
    JZ error_exit
    MOV ECX, ESI
    LOAD_IMM EDX, 0
    DIV EAX, ECX
    JMP print_result

print_result:
    LOAD_IMM EDX, 0
    LOAD_IMM EDI, result_buffer

    CMP EAX, 0
    JGE count_tens

    LOAD_IMM EBX, 45
    STORER EDI, EBX
    INC EDI
    INC EDX
    LOAD_IMM EBX, 0
    SUB EBX, EAX
    MOV EAX, EBX

count_tens:
    LOAD_IMM ECX, 0

tens_loop:
    CMP EAX, 10
    JL store_digits
    LOAD_IMM EBX, 10
    SUB EAX, EBX
    INC ECX
    JMP tens_loop

store_digits:
    CMP ECX, 0
    JZ store_ones
    LOAD_IMM EBX, 48
    ADD ECX, EBX
    STORER EDI, ECX
    INC EDI
    INC EDX

store_ones:
    LOAD_IMM EBX, 48
    ADD EAX, EBX
    STORER EDI, EAX
    MOV ESI, EDX
    INC ESI

    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, result_msg
    LOAD_IMM EDX, result_msg_len
    INT 0x80

    MOV EDX, ESI
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, result_buffer
    INT 0x80

    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, newline
    LOAD_IMM EDX, 1
    INT 0x80
    JMP exit_program

error_exit:
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, error_msg
    LOAD_IMM EDX, error_msg_len
    INT 0x80

exit_program:
    LOAD_IMM EAX, 1
    LOAD_IMM EBX, 0
    INT 0x80
    HALT
