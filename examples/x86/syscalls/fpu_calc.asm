; ==========================================
; FPU Calculator Demo (x86 32-bit)
; ==========================================
; Exercises native x87-backed codegen with integer-to-float load, arithmetic,
; and integer store-back for display.

.data
	intro: DB "fpu_calc: x87 pipeline demo", 10, 0
	intro_len EQU 28
	prefix: DB "((7 + 5) * 2) / 3 = ", 0
	prefix_len EQU 20
	newline: DB 10
	result_int: RESB 4
	result_buffer: RESB 2

.text
_start:
	FINIT
	FILD 7
	FADD 5
	FMUL 2
	FDIV 3
	FISTP result_int

	LOAD_IMM ESI, result_int
	LOADR EAX, ESI
	LOAD_IMM EBX, 48
	ADD EAX, EBX
	LOAD_IMM ESI, result_buffer
	STORER ESI, EAX

	LOAD_IMM EAX, 4
	LOAD_IMM EBX, 1
	LOAD_IMM ECX, intro
	LOAD_IMM EDX, intro_len
	INT 0x80

	LOAD_IMM EAX, 4
	LOAD_IMM EBX, 1
	LOAD_IMM ECX, prefix
	LOAD_IMM EDX, prefix_len
	INT 0x80

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

	LOAD_IMM EAX, 1
	LOAD_IMM EBX, 0
	INT 0x80
	HALT
