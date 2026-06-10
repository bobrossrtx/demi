; ==========================================
; Conditional Jumps (x64 64-bit)
; ==========================================
; Demonstrates various conditional jump instructions
; Uses 64-bit comparison operations

.text

_start:
    ; Test 1: Jump if equal (JE/JZ)
    LOAD_IMM RAX, 10
    LOAD_IMM RBX, 10
    CMP RAX, RBX
    JE equal_branch     ; Should jump (10 == 10)
    LOAD_IMM RCX, 99    ; Should not execute
    
equal_branch:
    LOAD_IMM RCX, 1     ; Mark test 1 passed
    
    ; Test 2: Jump if not equal (JNE/JNZ)
    LOAD_IMM RAX, 5
    LOAD_IMM RBX, 10
    CMP RAX, RBX
    JNE not_equal_branch ; Should jump (5 != 10)
    LOAD_IMM RDX, 99     ; Should not execute
    
not_equal_branch:
    LOAD_IMM RDX, 2      ; Mark test 2 passed
    
    ; Test 3: Jump if greater (JG)
    LOAD_IMM RAX, 15
    LOAD_IMM RBX, 10
    CMP RAX, RBX
    JG greater_branch    ; Should jump (15 > 10)
    LOAD_IMM RSI, 99     ; Should not execute
    
greater_branch:
    LOAD_IMM RSI, 3      ; Mark test 3 passed
    
    ; Test 4: Jump if less (JL)
    LOAD_IMM RAX, 3
    LOAD_IMM RBX, 10
    CMP RAX, RBX
    JL less_branch       ; Should jump (3 < 10)
    LOAD_IMM RDI, 99     ; Should not execute
    
less_branch:
    LOAD_IMM RDI, 4      ; Mark test 4 passed
    
    LOAD_IMM RAX, 79
    OUT RAX, 1
    LOAD_IMM RAX, 75
    OUT RAX, 1
    LOAD_IMM RAX, 10
    OUT RAX, 1
    HALT

; Results: RCX=1, RDX=2, RSI=3, RDI=4
