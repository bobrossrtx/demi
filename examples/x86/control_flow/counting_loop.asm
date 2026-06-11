; ==========================================
; Counting Loop Example (x86 32-bit)
; ==========================================
; This program demonstrates a counting loop
; Counts from 1 to 5 and outputs each number
; Uses 32-bit registers

.data
prefix_msg: DB 'counting_loop: ', 0

.text
.org 0x100

_start:
    LOAD_IMM EDX, prefix_msg
    OUTSTR EDX, 1

    ; Initialize counter in EAX
    LOAD_IMM EAX, 1     ; Start from 1
    LOAD_IMM EBX, 5     ; Loop limit
    
loop_start:
    ; Output current counter value (as ASCII character)
    ; Add 48 to convert number to ASCII digit
    MOV ECX, EAX        ; Copy counter to ECX
    LOAD_IMM EDX, 48    ; ASCII offset for '0'
    ADD ECX, EDX        ; Convert to ASCII digit
    OUT ECX, 1          ; Output to console
    
    ; Output a space
    LOAD_IMM ECX, 32    ; ASCII space
    OUT ECX, 1
    
    ; Increment counter
    INC EAX
    
    ; Check if we've reached the limit
    CMP EAX, EBX
    JLE loop_start      ; Jump if EAX <= EBX
    
    ; Output newline at end
    LOAD_IMM EAX, 10    ; ASCII newline
    OUT EAX, 1
    
    ; End program
    HALT

; Expected output: "counting_loop: 1 2 3 4 5 \n"
