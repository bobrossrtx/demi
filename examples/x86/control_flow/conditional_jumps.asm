; ==========================================
; Conditional Jumps (x86 32-bit)
; ==========================================
; Demonstrates various conditional jump instructions
; Uses 32-bit comparison operations

.data
result_msg: DB 'conditional_jumps: ecx=1, edx=2, esi=3, edi=4', 10, 0

.text
.org 0x100

_start:
    ; Test 1: Jump if equal (JE/JZ)
    LOAD_IMM EAX, 10
    LOAD_IMM EBX, 10
    CMP EAX, EBX
    JE equal_branch     ; Should jump (10 == 10)
    LOAD_IMM ECX, 99    ; Should not execute
    
equal_branch:
    LOAD_IMM ECX, 1     ; Mark test 1 passed
    
    ; Test 2: Jump if not equal (JNE/JNZ)
    LOAD_IMM EAX, 5
    LOAD_IMM EBX, 10
    CMP EAX, EBX
    JNE not_equal_branch ; Should jump (5 != 10)
    LOAD_IMM EDX, 99     ; Should not execute
    
not_equal_branch:
    LOAD_IMM EDX, 2      ; Mark test 2 passed
    
    ; Test 3: Jump if greater (JG)
    LOAD_IMM EAX, 15
    LOAD_IMM EBX, 10
    CMP EAX, EBX
    JG greater_branch    ; Should jump (15 > 10)
    LOAD_IMM ESI, 99     ; Should not execute
    
greater_branch:
    LOAD_IMM ESI, 3      ; Mark test 3 passed
    
    ; Test 4: Jump if less (JL)
    LOAD_IMM EAX, 3
    LOAD_IMM EBX, 10
    CMP EAX, EBX
    JL less_branch       ; Should jump (3 < 10)
    LOAD_IMM EDI, 99     ; Should not execute
    
less_branch:
    LOAD_IMM EDI, 4      ; Mark test 4 passed
    
    ; Print labeled branch results
    LOAD_IMM EAX, result_msg
    OUTSTR EAX, 1
    HALT

; Results: ECX=1, EDX=2, ESI=3, EDI=4
; Output: "conditional_jumps: ecx=1, edx=2, esi=3, edi=4\n"
