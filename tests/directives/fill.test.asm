; ========================================
; Test: .fill directive
; Verifies multi-byte fill with repeat
; ========================================
.test name="fill_directive" description="Tests .fill count,size,value"

.section .data
.org 0x100
start:  DB 0xFF
.fill 2, 4, 0xDEADBEEF   ; 2 repeats of 4-byte value
mid:    DB 0x00
.fill 3, 1, 0x42          ; 3 repeats of 1-byte value
end:    DB 0xCC

.section .text
.global _start
_start:
    ; Verify start marker
    LOAD_IMM RAX, [start]
    DEBUG ASSERT RAX, 0xFF
    ; Verify first fill: DEADBEEF at 0x101 (little-endian: EF BE AD DE)
    LOAD_IMM RAX, 0x101
    LOADL RAX, [RAX]
    DEBUG ASSERT RAX, 0xDEADBEEF
    ; Verify second fill: DEADBEEF at 0x105
    LOAD_IMM RAX, 0x105
    LOADL RAX, [RAX]
    DEBUG ASSERT RAX, 0xDEADBEEF
    ; Verify mid marker at 0x109
    LOAD_IMM RAX, [mid]
    DEBUG ASSERT RAX, 0x00
    ; Verify 1-byte fills at 0x10A-0x10C are 0x42
    LOAD_IMM RAX, 0x10A
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 0x42
    LOAD_IMM RAX, 0x10B
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 0x42
    LOAD_IMM RAX, 0x10C
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 0x42
    ; Verify end marker at 0x10D
    LOAD_IMM RAX, [end]
    DEBUG ASSERT RAX, 0xCC
    HALT
