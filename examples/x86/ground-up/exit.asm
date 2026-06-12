; Programming from the Ground Up — Chapter 3
; exit.s: Minimal exit program
; Original: movl $1, %eax / movl $0, %ebx / int $0x80
;
; Assembles with: --assembly-target x86-elf32

.section .text
.global _start
_start:
    MOV EAX, 1      ; sys_exit
    MOV EBX, 0      ; exit code 0
    INT 0x80
