; ==========================================
; FPU Showcase (x64 64-bit) — verified subset
; ==========================================
; Tests the FPU instructions that are verified working in both
; -A (VM) and -o (native) modes.
;
; Verified:    FINIT  FCLEX  FSTSW(R0)  FILD  FISTP
;              FLD    FADD   FSUB       FMUL  FDIV
;              FSIN   FCOS   FSQRT
;
; Expected Output: FPU showcase: all verified ops passed

.data
.org 0x50
msg: DB "FPU showcase: all verified ops passed", 10, 0
step_finit: DB "[fpu] FINIT FCLEX FSTSW OK", 10, 0
step_arith: DB "[fpu] FILD FADD FSUB FMUL FDIV OK", 10, 0
step_trig:  DB "[fpu] FSIN FCOS FSQRT OK", 10, 0

.org 0x200
val1: RESB 4
val2: RESB 4

.text
.org 0x280

_start:
    ; Test 1: FINIT + FCLEX + FSTSW(R0)
    FINIT
    FCLEX
    FSTSW R0
    LOAD_IMM RBX, 0
    CMP RAX, RBX
    JNZ fail
    LOAD_IMM RAX, step_finit
    OUTSTR RAX, 1

    ; Test 2: FILD + arithmetic
    FILD 7
    FADD 5
    FISTP val1
    LOAD RAX, val1
    LOAD_IMM RBX, 12
    CMP RAX, RBX
    JNZ fail

    FLD 20
    FSUB 8
    FISTP val1
    LOAD RAX, val1
    LOAD_IMM RBX, 12
    CMP RAX, RBX
    JNZ fail

    FLD 6
    FMUL 7
    FISTP val1
    LOAD RAX, val1
    LOAD_IMM RBX, 42
    CMP RAX, RBX
    JNZ fail

    FLD 100
    FDIV 4
    FISTP val1
    LOAD RAX, val1
    LOAD_IMM RBX, 25
    CMP RAX, RBX
    JNZ fail

    LOAD_IMM RAX, step_arith
    OUTSTR RAX, 1

    ; Test 3: Trigonometry
    FLD 0
    FSIN
    FISTP val1
    LOAD RAX, val1
    LOAD_IMM RBX, 0
    CMP RAX, RBX
    JNZ fail

    FLD 0
    FCOS
    FISTP val1
    LOAD RAX, val1
    LOAD_IMM RBX, 1
    CMP RAX, RBX
    JNZ fail

    FLD 49
    FSQRT
    FISTP val1
    LOAD RAX, val1
    LOAD_IMM RBX, 7
    CMP RAX, RBX
    JNZ fail

    LOAD_IMM RAX, step_trig
    OUTSTR RAX, 1

    ; All passed
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1
    HALT

fail:
    HALT
