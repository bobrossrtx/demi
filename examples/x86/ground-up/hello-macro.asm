; hello with macros
.include "macros.inc"

.section .data
hello: .string "Hello, World!\n"

.section .text
.global _start
_start:
    sys_write STDOUT, hello, 14
    sys_exit 0
