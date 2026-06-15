; ========================================
; Test: .irp directive (parameterized repeat)
; ========================================
.test name="irp_directive" description="Tests .irp parameter substitution"

.section .data
.org 0x100
.irp val, 0x10, 0x20, 0x30
    DB \val
.endr
; Should produce: 0x10, 0x20, 0x30 at 0x100-0x102

.irpc ch, "ABC"
    DB \ch
.endr
; Should produce: 'A', 'B', 'C' at 0x103-0x105

.section .text
.global _start
_start:
    LOAD_IMM RAX, 0x100
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 0x10
    LOAD_IMM RAX, 0x101
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 0x20
    LOAD_IMM RAX, 0x102
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 0x30
    LOAD_IMM RAX, 0x103
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 65   ; 'A'
    LOAD_IMM RAX, 0x104
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 66   ; 'B'
    LOAD_IMM RAX, 0x105
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 67   ; 'C'
    HALT
