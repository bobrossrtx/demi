; ==========================================
; Simple Calculator (x86 32-bit)
; ==========================================
; Interactive calculator that adds two single-digit numbers
; Demonstrates string-to-integer conversion for simple cases

.data
    prompt1: DB "Enter first digit (0-9): ", 0
    prompt1_len EQU 25
    prompt2: DB "Enter second digit (0-9): ", 0
    prompt2_len EQU 26
    result_msg: DB "Sum: ", 0
    result_msg_len EQU 5
    newline: DB 10
    
    input_buffer: RESB 10

.text
_start:
    ; === Get first digit ===
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, prompt1
    LOAD_IMM EDX, prompt1_len
    INT 0x80
    
    ; Read first digit
    LOAD_IMM EAX, 3
    LOAD_IMM EBX, 0
    LOAD_IMM ECX, input_buffer
    LOAD_IMM EDX, 10
    INT 0x80
    
    ; Convert ASCII to number: load byte, mask, subtract '0' (48)
    LOAD EAX, input_buffer
    LOAD_IMM EBX, 0xFF
    AND EAX, EBX            ; Mask to single byte
    LOAD_IMM EBX, 48
    SUB EAX, EBX
    MOV EDI, EAX            ; Store first number in EDI
    
    ; === Get second digit ===
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, prompt2
    LOAD_IMM EDX, prompt2_len
    INT 0x80
    
    ; Read second digit
    LOAD_IMM EAX, 3
    LOAD_IMM EBX, 0
    LOAD_IMM ECX, input_buffer
    LOAD_IMM EDX, 10
    INT 0x80
    
    ; Convert ASCII to number
    LOAD EAX, input_buffer
    LOAD_IMM EBX, 0xFF
    AND EAX, EBX            ; Mask to single byte
    LOAD_IMM EBX, 48
    SUB EAX, EBX
    
    ; === Calculate sum ===
    ADD EAX, EDI            ; Add first number to second
    
    ; === Convert result to ASCII and display ===
    ; Print "Sum: "
    PUSH EAX                ; Save result
    
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, result_msg
    LOAD_IMM EDX, result_msg_len
    INT 0x80
    
    POP EAX                 ; Restore result
    
    ; Convert number to ASCII (add 48)
    LOAD_IMM EBX, 48
    ADD EAX, EBX
    
    ; Store result character in buffer
    STORE EAX, input_buffer
    
    ; Print result digit
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, input_buffer
    LOAD_IMM EDX, 1
    INT 0x80
    
    ; Print newline
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
