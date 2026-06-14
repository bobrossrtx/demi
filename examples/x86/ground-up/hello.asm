; Programming from the Ground Up — Chapter 3
; hello.s: Hello World using write syscall
; Original: writes a greeting string to stdout
;
; Expected Output:
;   Hello, World!

.section .data
hello:
    .string "Hello, World!\n"

.section .text
.global _start
_start:
    MOV EAX, 4          ; sys_write
    MOV EBX, 1          ; stdout
    MOV ECX, hello      ; string address
    MOV EDX, 14         ; string length
    INT 0x80

    MOV EAX, 1          ; sys_exit
    MOV EBX, 0
    INT 0x80
