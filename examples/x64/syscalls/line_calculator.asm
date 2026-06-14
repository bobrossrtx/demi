; ==========================================
; Line Calculator (x64 64-bit) — OUT-based, single-shot
; ==========================================
; Reads one expression (e.g. "12 + 4"), prints result, exits.
; Uses OUT/OUTSTR for output (works in both -A and -o modes).
; Single INT 0x80 for reading.
;
; Usage: echo "12 + 4" | ./line_calculator
; Output: 12 + 4 = 16

.data
.org 0x50
prompt: DB "calc> ", 0
eq:     DB " = ", 0
err_msg: DB " Error", 10, 0

.org 0x80
input_buf: RESB 128

.text
.org 0x100

_start:
    ; Print prompt
    LOAD_IMM RAX, prompt
    OUTSTR RAX, 1

    ; Read one line
    LOAD_IMM RAX, 3
    LOAD_IMM RBX, 0
    LOAD_IMM RCX, input_buf
    LOAD_IMM RDX, 127
    INT 0x80
    CMP RAX, 0
    JLE done

    ; NUL-terminate, stripping trailing newline
    LOAD_IMM RDI, input_buf
    ADD RDI, RAX
    DEC RDI
    LOADR RBX, RDI
    CMP RBX, 10           ; newline?
    JNZ term_ok
    LOAD_IMM RBX, 0
    STORER RDI, RBX       ; replace newline with NUL
    JMP term_done
term_ok:
    INC RDI               ; no newline, NUL after last char
    LOAD_IMM RBX, 0
    STORER RDI, RBX
term_done:

    LOAD_IMM RSI, input_buf

    ; === skip_spaces ===
skip1:
    LOADR RAX, RSI
    CMP RAX, 32
    JZ skip1_inc
    CMP RAX, 9
    JZ skip1_inc
    JMP skip1_done
skip1_inc:
    INC RSI
    JMP skip1
skip1_done:

    ; Quit check
    LOADR RAX, RSI
    CMP RAX, 113
    JZ done
    CMP RAX, 81
    JZ done

    ; === parse first number (unsigned, positive) ===
    LOAD_IMM RDI, 0
p1:
    LOADR RAX, RSI
    CMP RAX, 48
    JL p1_done
    CMP RAX, 57
    JG p1_done
    ; RDI = RDI*10 + digit
    LOAD_IMM RCX, 48
    SUB RAX, RCX
    MOV R8, RDI
    LOAD_IMM RCX, 9
p1_m10:
    CMP RCX, 0
    JE p1_m10d
    ADD RDI, R8
    DEC RCX
    JMP p1_m10
p1_m10d:
    ADD RDI, RAX
    INC RSI
    JMP p1
p1_done:

    ; === skip_spaces ===
skip2:
    LOADR RAX, RSI
    CMP RAX, 32
    JZ skip2_inc
    CMP RAX, 9
    JZ skip2_inc
    JMP skip2_done
skip2_inc:
    INC RSI
    JMP skip2
skip2_done:

    ; Get operator
    LOADR RBX, RSI
    INC RSI

    ; === skip_spaces ===
skip3:
    LOADR RAX, RSI
    CMP RAX, 32
    JZ skip3_inc
    CMP RAX, 9
    JZ skip3_inc
    JMP skip3_done
skip3_inc:
    INC RSI
    JMP skip3
skip3_done:

    ; === parse second number ===
    LOAD_IMM RCX, 0
p2:
    LOADR RAX, RSI
    CMP RAX, 48
    JL p2_done
    CMP RAX, 57
    JG p2_done
    LOAD_IMM RDX, 48
    SUB RAX, RDX
    MOV R8, RCX
    LOAD_IMM RDX, 9
p2_m10:
    CMP RDX, 0
    JE p2_m10d
    ADD RCX, R8
    DEC RDX
    JMP p2_m10
p2_m10d:
    ADD RCX, RAX
    INC RSI
    JMP p2
p2_done:

    ; === Compute ===
    MOV RAX, RDI
    CMP RBX, 43
    JZ do_add
    CMP RBX, 45
    JZ do_sub
    CMP RBX, 42
    JZ do_mul
    CMP RBX, 47
    JZ do_div
    JMP error

do_add:
    ADD RAX, RCX
    JMP print_result
do_sub:
    SUB RAX, RCX
    JMP print_result
do_mul:
    MOV R8, RAX
    LOAD_IMM RAX, 0
    CMP RCX, 0
    JZ print_result
mul_lp:
    ADD RAX, R8
    DEC RCX
    JNZ mul_lp
    JMP print_result
do_div:
    CMP RCX, 0
    JZ error
    LOAD_IMM RDX, 0
    DIV RAX, RAX, RCX
    JMP print_result

print_result:
    ; Save result first (OUTSTR clobbers RAX)
    MOV R8, RAX

    ; Echo the input expression
    LOAD_IMM RAX, input_buf
    OUTSTR RAX, 1

    ; Print " = "
    LOAD_IMM RAX, eq
    OUTSTR RAX, 1

    ; Handle negative
    CMP R8, 0
    JGE pr_pos
    LOAD_IMM RAX, 45     ; '-'
    OUT RAX, 1
    LOAD_IMM RAX, 0
    SUB RAX, R8
    MOV R8, RAX

pr_pos:
    ; Extract tens digit (for values < 100)
    LOAD_IMM RCX, 10
    LOAD_IMM RDI, 0      ; tens counter
    MOV RAX, R8
pr_tens:
    CMP RAX, RCX
    JL pr_tens_done
    SUB RAX, RCX
    INC RDI
    JMP pr_tens
pr_tens_done:
    ; If tens > 0, print it
    CMP RDI, 0
    JZ pr_ones
    LOAD_IMM RBX, 48
    ADD RDI, RBX
    OUT RDI, 1

pr_ones:
    ; Print ones digit (RAX has remainder)
    LOAD_IMM RBX, 48
    ADD RAX, RBX
    OUT RAX, 1

    ; Newline and loop
    LOAD_IMM RAX, 10
    OUT RAX, 1
    JMP _start

done:
    LOAD_IMM RAX, 10     ; newline
    OUT RAX, 1
    HALT

error:
    LOAD_IMM RAX, err_msg
    OUTSTR RAX, 1
    JMP _start
