; ========================================  
; Test: .p2align directive
; Verifies power-of-2 alignment with fill
; ========================================
.test name="p2align_directive" description="Tests .p2align alignment"

.section .data
.org 0x100
DB 0x01                  ; one byte at 0x100
.p2align 4, 0xCC         ; align to 2^4=16, fill with 0xCC
aligned: DB 0xAA         ; should be at 0x110 (aligned to 16)

.section .text
.global _start
_start:
    ; Verify padding bytes at 0x101-0x10F are 0xCC
    LOAD_IMM RAX, 0x101
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 0xCC
    LOAD_IMM RAX, 0x10F
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 0xCC
    ; Verify aligned marker at 0x110
    LOAD_IMM RAX, [aligned]
    DEBUG ASSERT RAX, 0xAA
    HALT
