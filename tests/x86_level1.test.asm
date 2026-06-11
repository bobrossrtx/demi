; Level 1 Integration Tests — x86-32 Native Assembler
; Assemble with: demi-engine -A tests/x86_level1.test.asm --assembly-target x86-elf32 -o /dev/null
; Or: make test-assembler (unit tests are in src/test/test_assembler.cpp)

.section .text
.global _start

_start:
    ; ---- L1.4: MUL/DIV ----
    MUL EBX            ; F7 E3 — unsigned multiply EAX * EBX → EDX:EAX
    DIV ECX            ; F7 F1 — unsigned divide EDX:EAX / ECX

    ; ---- L1.10: IMUL (3 forms) ----
    IMUL EBX           ; F7 EB — signed multiply (single operand)
    IMUL EAX, EBX      ; 0F AF C3 — signed multiply (two operand)
    IMUL EAX, EBX, 5   ; 6B C3 05 — signed multiply (three operand, imm8)
    IMUL EAX, EBX, 1000 ; 69 C3 E8 03 00 00 — signed multiply (three operand, imm32)

    ; ---- L1.5: Indirect CALL/JMP [mem] ----
    CALL [EAX]         ; FF 10 — indirect call through memory
    JMP [EBP-4]        ; FF 65 FC — indirect jump through memory

    ; ---- L1.7: .comm in data section ----
    ; (assembled separately in .data)

    ; Exit
    MOV EAX, 1
    MOV EBX, 0
    INT 0x80

.section .data
    ; ---- L1.6: GNU aliases ----
    .byte 1, 2         ; 01 02 — byte directive alias
    .word 0x1234       ; 34 12 — word directive alias
    .long 42           ; 2A 00 00 00 — long (dword) alias
    .quad 0xDEADBEEF   ; EF BE AD DE 00 00 00 00 — quad alias

.section .bss
    ; ---- L1.7: .comm ----
    .comm buf, 256     ; Reserve 256 bytes in BSS as global symbol 'buf'
