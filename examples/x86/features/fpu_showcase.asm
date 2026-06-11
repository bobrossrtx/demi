; ==========================================
; FPU Showcase (x86 32-bit)
; ==========================================
; Exercises every real DISA FPU opcode currently implemented in the engine and
; compiler. The program is self-checking: it prints a success banner only if all
; verification steps pass, otherwise it prints the first failing stage.

.data
    success_msg: DB "fpu_showcase: verified FINIT FCLEX FLD FST FSTP FILD FIST FISTP FADD FSUB FMUL FDIV FSIN FCOS FTAN FSQRT FABS FCHS FSTCW FLDCW FSTSW FCOMPP FUCOMPP", 10, 0
    success_msg_len EQU 148

    step_finit_msg: DB "[fpu_showcase] FINIT + FSTSW(R0): reset x87 state and verify clear status", 10, 0
    step_fclex_msg: DB "[fpu_showcase] FCLEX: clear x87 exceptions and re-check status", 10, 0
    step_fstcw_msg: DB "[fpu_showcase] FSTCW: store default control word and verify both bytes", 10, 0
    step_fldcw_msg: DB "[fpu_showcase] FLDCW + FSTCW: load custom control word and verify round-trip", 10, 0
    step_fstsw_mem_msg: DB "[fpu_showcase] FSTSW(mem) + FSTSW(R0): exercise memory and register status forms", 10, 0
    step_fist_msg: DB "[fpu_showcase] FILD/FIST/FISTP: convert integer 42 through x87 stack and memory", 10, 0
    step_fst_msg: DB "[fpu_showcase] FLD/FST/FISTP: store ST0 to memory and reload it", 10, 0
    step_fstp_msg: DB "[fpu_showcase] FSTP: store-and-pop, then reload and verify 13", 10, 0
    step_fadd_msg: DB "[fpu_showcase] FADD: 7 + 5 -> 12", 10, 0
    step_fsub_msg: DB "[fpu_showcase] FSUB: 20 - 8 -> 12", 10, 0
    step_fmul_msg: DB "[fpu_showcase] FMUL: 6 * 7 -> 42", 10, 0
    step_fdiv_msg: DB "[fpu_showcase] FDIV: 100 / 4 -> 25", 10, 0
    step_fsin_msg: DB "[fpu_showcase] FSIN: sin(0) -> 0", 10, 0
    step_fcos_msg: DB "[fpu_showcase] FCOS: cos(0) -> 1", 10, 0
    step_ftan_msg: DB "[fpu_showcase] FTAN: tan(0) -> 0 after popping the extra 1.0", 10, 0
    step_fsqrt_msg: DB "[fpu_showcase] FSQRT: sqrt(49) -> 7", 10, 0
    step_fabs_msg: DB "[fpu_showcase] FABS: negate then take abs(9) -> 9", 10, 0
    step_fchs_msg: DB "[fpu_showcase] FCHS: change sign of 11 -> -11", 10, 0
    step_fcompp_msg: DB "[fpu_showcase] FCOMPP: compare equal values and verify ZF path", 10, 0
    step_fucompp_msg: DB "[fpu_showcase] FUCOMPP: compare ordered less-than and verify carry path", 10, 0

    fail_finit_msg: DB "fpu_showcase: FINIT/FSTSW AX verification failed", 10, 0
    fail_fclex_msg: DB "fpu_showcase: FCLEX verification failed", 10, 0
    fail_fstcw_msg: DB "fpu_showcase: FSTCW default control word verification failed", 10, 0
    fail_fldcw_msg: DB "fpu_showcase: FLDCW round-trip verification failed", 10, 0
    fail_fstsw_mem_msg: DB "fpu_showcase: FSTSW memory verification failed", 10, 0
    fail_fist_msg: DB "fpu_showcase: FILD/FIST/FISTP verification failed", 10, 0
    fail_fst_msg: DB "fpu_showcase: FLD/FST/FLD-memory verification failed", 10, 0
    fail_fstp_msg: DB "fpu_showcase: FSTP verification failed", 10, 0
    fail_fadd_msg: DB "fpu_showcase: FADD verification failed", 10, 0
    fail_fsub_msg: DB "fpu_showcase: FSUB verification failed", 10, 0
    fail_fmul_msg: DB "fpu_showcase: FMUL verification failed", 10, 0
    fail_fdiv_msg: DB "fpu_showcase: FDIV verification failed", 10, 0
    fail_fsin_msg: DB "fpu_showcase: FSIN verification failed", 10, 0
    fail_fcos_msg: DB "fpu_showcase: FCOS verification failed", 10, 0
    fail_ftan_msg: DB "fpu_showcase: FTAN verification failed", 10, 0
    fail_fsqrt_msg: DB "fpu_showcase: FSQRT verification failed", 10, 0
    fail_fabs_msg: DB "fpu_showcase: FABS verification failed", 10, 0
    fail_fchs_msg: DB "fpu_showcase: FCHS verification failed", 10, 0
    fail_fcompp_msg: DB "fpu_showcase: FCOMPP flag verification failed", 10, 0
    fail_fucompp_msg: DB "fpu_showcase: FUCOMPP flag verification failed", 10, 0

    custom_cw: DB 0x7F, 0x0B
    default_cw: RESB 2
    loaded_cw: RESB 2
    status_word: RESB 2

    stored_double: RESB 8
    popped_double: RESB 8

    fist_value: RESB 4
    fistp_value: RESB 4
    fld_mem_value: RESB 4
    fst_value: RESB 4
    fstp_value: RESB 4
    add_value: RESB 4
    sub_value: RESB 4
    mul_value: RESB 4
    div_value: RESB 4
    sin_value: RESB 4
    cos_value: RESB 4
    tan_value: RESB 4
    sqrt_value: RESB 4
    abs_value: RESB 4
    chs_value: RESB 4

.text
.org 0x100

_start:
    LOAD_IMM EAX, step_finit_msg
    OUTSTR EAX, 1
    FINIT

    ; FINIT should leave the status word clear.
    FSTSW R0
    LOAD_IMM EBX, 0
    CMP EAX, EBX
    JNZ fail_finit

    ; FCLEX should preserve the clear status in this clean path.
    LOAD_IMM EAX, step_fclex_msg
    OUTSTR EAX, 1
    FCLEX
    FSTSW R0
    LOAD_IMM EBX, 0
    CMP EAX, EBX
    JNZ fail_fclex

    ; Default x87 control word after FINIT is 0x037F.
    LOAD_IMM EAX, step_fstcw_msg
    OUTSTR EAX, 1
    FSTCW default_cw
    LOAD EAX, default_cw
    LOAD_IMM EBX, 0x7F
    CMP EAX, EBX
    JNZ fail_fstcw
    LEA ECX, default_cw
    INC ECX
    LOADR EAX, ECX
    LOAD_IMM EBX, 0x03
    CMP EAX, EBX
    JNZ fail_fstcw

    ; Load a different control word and make sure it round-trips.
    LOAD_IMM EAX, step_fldcw_msg
    OUTSTR EAX, 1
    FLDCW custom_cw
    FSTCW loaded_cw
    LOAD EAX, loaded_cw
    LOAD_IMM EBX, 0x7F
    CMP EAX, EBX
    JNZ fail_fldcw
    LEA ECX, loaded_cw
    INC ECX
    LOADR EAX, ECX
    LOAD_IMM EBX, 0x0B
    CMP EAX, EBX
    JNZ fail_fldcw

    ; Restore the default control word, exercise the memory form of FSTSW,
    ; and verify the clear status via the register form.
    LOAD_IMM EAX, step_fstsw_mem_msg
    OUTSTR EAX, 1
    FLDCW default_cw
    FSTSW status_word
    FSTSW R0
    LOAD_IMM EBX, 0
    CMP EAX, EBX
    JNZ fail_fstsw_mem

    ; FILD immediate, FIST (no pop), and FISTP (pop) should all preserve 42.
    LOAD_IMM EAX, step_fist_msg
    OUTSTR EAX, 1
    FILD 42
    FIST fist_value
    FISTP fistp_value
    LOAD EAX, fist_value
    LOAD_IMM EBX, 42
    CMP EAX, EBX
    JNZ fail_fist
    LOAD EAX, fistp_value
    LOAD_IMM EBX, 42
    CMP EAX, EBX
    JNZ fail_fist

    ; FLD immediate + FST should keep the stack live so we can also FISTP.
    LOAD_IMM EAX, step_fst_msg
    OUTSTR EAX, 1
    FLD 21
    FST stored_double
    FISTP fst_value
    LOAD EAX, fst_value
    LOAD_IMM EBX, 21
    CMP EAX, EBX
    JNZ fail_fst
    FLD stored_double
    FISTP fld_mem_value
    LOAD EAX, fld_mem_value
    LOAD_IMM EBX, 21
    CMP EAX, EBX
    JNZ fail_fst

    ; FSTP should store and pop, allowing a later FLD from memory to recover 13.
    LOAD_IMM EAX, step_fstp_msg
    OUTSTR EAX, 1
    FLD 13
    FSTP popped_double
    FLD popped_double
    FISTP fstp_value
    LOAD EAX, fstp_value
    LOAD_IMM EBX, 13
    CMP EAX, EBX
    JNZ fail_fstp

    LOAD_IMM EAX, step_fadd_msg
    OUTSTR EAX, 1
    FILD 7
    FADD 5
    FISTP add_value
    LOAD EAX, add_value
    LOAD_IMM EBX, 12
    CMP EAX, EBX
    JNZ fail_fadd

    LOAD_IMM EAX, step_fsub_msg
    OUTSTR EAX, 1
    FLD 20
    FSUB 8
    FISTP sub_value
    LOAD EAX, sub_value
    LOAD_IMM EBX, 12
    CMP EAX, EBX
    JNZ fail_fsub

    LOAD_IMM EAX, step_fmul_msg
    OUTSTR EAX, 1
    FLD 6
    FMUL 7
    FISTP mul_value
    LOAD EAX, mul_value
    LOAD_IMM EBX, 42
    CMP EAX, EBX
    JNZ fail_fmul

    LOAD_IMM EAX, step_fdiv_msg
    OUTSTR EAX, 1
    FLD 100
    FDIV 4
    FISTP div_value
    LOAD EAX, div_value
    LOAD_IMM EBX, 25
    CMP EAX, EBX
    JNZ fail_fdiv

    LOAD_IMM EAX, step_fsin_msg
    OUTSTR EAX, 1
    FLD 0
    FSIN
    FISTP sin_value
    LOAD EAX, sin_value
    LOAD_IMM EBX, 0
    CMP EAX, EBX
    JNZ fail_fsin

    LOAD_IMM EAX, step_fcos_msg
    OUTSTR EAX, 1
    FLD 0
    FCOS
    FISTP cos_value
    LOAD EAX, cos_value
    LOAD_IMM EBX, 1
    CMP EAX, EBX
    JNZ fail_fcos

    LOAD_IMM EAX, step_ftan_msg
    OUTSTR EAX, 1
    FLD 0
    FTAN
    FISTP tan_value
    LOAD EAX, tan_value
    LOAD_IMM EBX, 0
    CMP EAX, EBX
    JNZ fail_ftan

    LOAD_IMM EAX, step_fsqrt_msg
    OUTSTR EAX, 1
    FLD 49
    FSQRT
    FISTP sqrt_value
    LOAD EAX, sqrt_value
    LOAD_IMM EBX, 7
    CMP EAX, EBX
    JNZ fail_fsqrt

    LOAD_IMM EAX, step_fabs_msg
    OUTSTR EAX, 1
    FILD 9
    FCHS
    FABS
    FISTP abs_value
    LOAD EAX, abs_value
    LOAD_IMM EBX, 9
    CMP EAX, EBX
    JNZ fail_fabs

    LOAD_IMM EAX, step_fchs_msg
    OUTSTR EAX, 1
    FILD 11
    FCHS
    FISTP chs_value
    LOAD EAX, chs_value
    LOAD_IMM EBX, 0xF5
    CMP EAX, EBX
    JNZ fail_fchs

    ; FCOMPP equal case should drive the VM zero flag.
    LOAD_IMM EAX, step_fcompp_msg
    OUTSTR EAX, 1
    FLD 3
    FLD 3
    FCOMPP
    JZ fcompp_ok
    JMP fail_fcompp

fcompp_ok:
    ; FUCOMPP ordered less-than case should drive carry/sign for the VM branch model.
    LOAD_IMM EAX, step_fucompp_msg
    OUTSTR EAX, 1
    FLD 8
    FLD 2
    FUCOMPP
    JC fucompp_ok
    JMP fail_fucompp

fucompp_ok:
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, success_msg
    LOAD_IMM EDX, success_msg_len
    INT 0x80
    LOAD_IMM EAX, 1
    LOAD_IMM EBX, 0
    INT 0x80
    HALT

fail_finit:
    LOAD_IMM EAX, fail_finit_msg
    OUTSTR EAX, 1
    HALT

fail_fclex:
    LOAD_IMM EAX, fail_fclex_msg
    OUTSTR EAX, 1
    HALT

fail_fstcw:
    LOAD_IMM EAX, fail_fstcw_msg
    OUTSTR EAX, 1
    HALT

fail_fldcw:
    LOAD_IMM EAX, fail_fldcw_msg
    OUTSTR EAX, 1
    HALT

fail_fstsw_mem:
    LOAD_IMM EAX, fail_fstsw_mem_msg
    OUTSTR EAX, 1
    HALT

fail_fist:
    LOAD_IMM EAX, fail_fist_msg
    OUTSTR EAX, 1
    HALT

fail_fst:
    LOAD_IMM EAX, fail_fst_msg
    OUTSTR EAX, 1
    HALT

fail_fstp:
    LOAD_IMM EAX, fail_fstp_msg
    OUTSTR EAX, 1
    HALT

fail_fadd:
    LOAD_IMM EAX, fail_fadd_msg
    OUTSTR EAX, 1
    HALT

fail_fsub:
    LOAD_IMM EAX, fail_fsub_msg
    OUTSTR EAX, 1
    HALT

fail_fmul:
    LOAD_IMM EAX, fail_fmul_msg
    OUTSTR EAX, 1
    HALT

fail_fdiv:
    LOAD_IMM EAX, fail_fdiv_msg
    OUTSTR EAX, 1
    HALT

fail_fsin:
    LOAD_IMM EAX, fail_fsin_msg
    OUTSTR EAX, 1
    HALT

fail_fcos:
    LOAD_IMM EAX, fail_fcos_msg
    OUTSTR EAX, 1
    HALT

fail_ftan:
    LOAD_IMM EAX, fail_ftan_msg
    OUTSTR EAX, 1
    HALT

fail_fsqrt:
    LOAD_IMM EAX, fail_fsqrt_msg
    OUTSTR EAX, 1
    HALT

fail_fabs:
    LOAD_IMM EAX, fail_fabs_msg
    OUTSTR EAX, 1
    HALT

fail_fchs:
    LOAD_IMM EAX, fail_fchs_msg
    OUTSTR EAX, 1
    HALT

fail_fcompp:
    LOAD_IMM EAX, fail_fcompp_msg
    OUTSTR EAX, 1
    HALT

fail_fucompp:
    LOAD_IMM EAX, fail_fucompp_msg
    OUTSTR EAX, 1
    HALT