; ==========================================
; Simple Calculator (x86 32-bit)
; ==========================================
; Interactive calculator that adds two single-digit numbers.

.data
    prompt1: DB "Enter first digit (0-9): ", 0
    prompt1_len EQU 25
    prompt2: DB "Enter second digit (0-9): ", 0
    prompt2_len EQU 26
    result_msg: DB "simple_calculator result: ", 0
    result_msg_len EQU 26
    newline: DB 10

    input_a: RESB 2
    input_b: RESB 2
    result_buffer: RESB 2

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
    LOAD_IMM ECX, input_b
    LOAD_IMM EDX, 2
    INT 0x80
    CMP EAX, 0
    JLE exit_program

    LOAD_IMM ESI, input_a
    LOADR EAX, ESI
    LOAD_IMM EBX, 0xFF
    AND EAX, EBX
    LOAD_IMM EBX, 48
    SUB EAX, EBX
    MOV EDI, EAX

    LOAD_IMM ESI, input_b
    LOADR EAX, ESI
    LOAD_IMM EBX, 0xFF
    AND EAX, EBX
    LOAD_IMM EBX, 48
    SUB EAX, EBX
    ADD EAX, EDI
    MOV EDI, EAX

    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, result_msg
    LOAD_IMM EDX, result_msg_len
    INT 0x80

    MOV EAX, EDI
    LOAD_IMM ESI, result_buffer
    LOAD_IMM EBX, 48
    ADD EAX, EBX
    STORER ESI, EAX

    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, result_buffer
    LOAD_IMM EDX, 1
    INT 0x80

    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, newline
    LOAD_IMM EDX, 1
    INT 0x80

exit_program:
    LOAD_IMM EAX, 1
    LOAD_IMM EBX, 0
    INT 0x80
    HALT
