; ==========================================
; Simple Calculator (x64 64-bit) — inline, no MUL64
; ==========================================
; Interactive calculator. All helpers inlined without CALL or MUL64.
; Usage: echo "5" | ./calculator  (one number at a time)

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
    error_msg: DB "Error", 10, 0
    error_msg_len EQU 7
    
    input_buffer: RESB 32
    operator_buffer: RESB 2
    result_buffer: RESB 32
    count_save: RESB 1

.text
_start:
    ; === Get first number ===
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, prompt1
    LOAD_IMM RDX, prompt1_len
    INT 0x80
    
    LOAD_IMM RAX, 3
    LOAD_IMM RBX, 0
    LOAD_IMM RCX, input_buffer
    LOAD_IMM RDX, 32
    INT 0x80
    CMP RAX, 0
    JLE exit_program
    
    ; --- atoi: input_buffer -> RDI (repeated add, no MUL64) ---
    LOAD_IMM RSI, input_buffer
    LOAD_IMM RDI, 0
atoi1_loop:
    LOADR RBX, RSI
    CMP RBX, 10
    JZ atoi1_done
    CMP RBX, 0
    JZ atoi1_done
    CMP RBX, 48
    JL atoi1_done
    CMP RBX, 57
    JG atoi1_done
    LOAD_IMM R8, 48
    SUB RBX, R8             ; digit value
    ; RDI = RDI * 10 + digit via repeated add
    MOV RAX, RDI
    LOAD_IMM RCX, 9
mul10_1:
    CMP RCX, 0
    JE mul10_1_done
    ADD RDI, RAX
    DEC RCX
    JMP mul10_1
mul10_1_done:
    ADD RDI, RBX
    INC RSI
    JMP atoi1_loop
atoi1_done:
    
    ; === Get operator ===
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, prompt2
    LOAD_IMM RDX, prompt2_len
    INT 0x80
    
    LOAD_IMM RAX, 3
    LOAD_IMM RBX, 0
    LOAD_IMM RCX, operator_buffer
    LOAD_IMM RDX, 2
    INT 0x80
    CMP RAX, 0
    JLE exit_program
    
    ; === Get second number ===
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, prompt3
    LOAD_IMM RDX, prompt3_len
    INT 0x80
    
    LOAD_IMM RAX, 3
    LOAD_IMM RBX, 0
    LOAD_IMM RCX, input_buffer
    LOAD_IMM RDX, 32
    INT 0x80
    CMP RAX, 0
    JLE exit_program
    
    ; --- atoi: input_buffer -> RSI (second number) ---
    LOAD_IMM RBP, 0         ; second number accumulator
    LOAD_IMM RSI, input_buffer
atoi2_loop:
    LOADR RBX, RSI
    CMP RBX, 10
    JZ atoi2_done
    CMP RBX, 0
    JZ atoi2_done
    CMP RBX, 48
    JL atoi2_done
    CMP RBX, 57
    JG atoi2_done
    LOAD_IMM R8, 48
    SUB RBX, R8
    MOV RAX, RBP
    LOAD_IMM RCX, 9
mul10_2:
    CMP RCX, 0
    JE mul10_2_done
    ADD RBP, RAX
    DEC RCX
    JMP mul10_2
mul10_2_done:
    ADD RBP, RBX
    INC RSI
    JMP atoi2_loop
atoi2_done:
    MOV RSI, RBP            ; second number in RSI
    
    ; === Perform calculation ===
    MOV RAX, RDI
    MOV RCX, RSI
    LOAD_IMM RDX, operator_buffer
    LOADR RBX, RDX
    
    CMP RBX, 43             ; '+'
    JZ do_add
    CMP RBX, 45             ; '-'
    JZ do_sub
    CMP RBX, 42             ; '*'
    JZ do_mul
    CMP RBX, 47             ; '/'
    JZ do_div
    JMP error_exit
    
do_add:
    ADD RAX, RCX
    JMP display_result
do_sub:
    SUB RAX, RCX
    JMP display_result
do_mul:
    ; multiply via repeated addition
    MOV R8, RAX
    LOAD_IMM RAX, 0
    CMP RCX, 0
    JZ mul_done
mul_loop:
    ADD RAX, R8
    DEC RCX
    JNZ mul_loop
mul_done:
    JMP display_result
do_div:
    CMP RCX, 0
    JZ error_exit
    LOAD_IMM RDX, 0
    DIV RAX, RAX, RCX
    JMP display_result
    
display_result:
    ; Build result string by repeated division
    LOAD_IMM RSI, result_buffer
    MOV RDI, RAX
    LOAD_IMM RCX, 0
    
    CMP RAX, 0
    JNZ itoa_loop
    LOAD_IMM RBX, 48
    STORER RSI, RBX
    LOAD_IMM RCX, 1
    JMP print_result
    
itoa_loop:
    LOAD_IMM RDX, 0
    LOAD_IMM RBP, 10
    DIV RAX, RAX, RBP
    LOAD_IMM R8, 48
    ADD RDX, R8
    STORER RSI, RDX
    INC RSI
    INC RCX
    CMP RAX, 0
    JNZ itoa_loop
    
    ; Reverse string in-place
    LOAD_IMM RDX, result_buffer
    DEC RSI
itoa_rev:
    CMP RDX, RSI
    JGE print_result
    LOADR RAX, RDX
    LOADR RBX, RSI
    STORER RDX, RBX
    STORER RSI, RAX
    INC RDX
    DEC RSI
    JMP itoa_rev
    
print_result:
    ; Print "Result: "
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, result_msg
    LOAD_IMM RDX, result_msg_len
    INT 0x80
    
    ; Print number — save count to memory before overwriting RCX
    LEA R8, count_save
    STORER R8, RCX
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, result_buffer
    LEA R8, count_save
    LOADR RDX, R8
    INT 0x80
    
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, newline
    LOAD_IMM RDX, 1
    INT 0x80
    
    JMP exit_program

error_exit:
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, error_msg
    LOAD_IMM RDX, error_msg_len
    INT 0x80

exit_program:
    LOAD_IMM RAX, 1
    LOAD_IMM RBX, 0
    INT 0x80
    HALT
