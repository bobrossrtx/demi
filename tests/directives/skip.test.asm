# Test: .skip directive
# Verifies byte skipping with fill pattern

.test "skip_directive" {
    .description "Tests .skip with and without fill byte"
    .category "Directives"

.section .data
.org 0x100
before: DB 0xAA
.skip 3, 0xBB
after: DB 0xCC

.section .text
.global _start
_start:
    LOAD_IMM RAX, [before]
    DEBUG ASSERT RAX, 0xAA
    LOAD_IMM RAX, 0x101
    LOAD RAX, [RAX]
    DEBUG ASSERT RAX, 0xBB
    LOAD_IMM RAX, [after]
    DEBUG ASSERT RAX, 0xCC
    HALT
}
