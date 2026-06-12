; Programming from the Ground Up — Ch 3-6 with macros

.macro sys_exit code
    MOV EAX, 1
    MOV EBX, code
    INT 0x80
.endm

.macro sys_write fd, buf, len
    MOV EAX, 4
    MOV EBX, fd
    MOV ECX, buf
    MOV EDX, len
    INT 0x80
.endm

.macro sys_read fd, buf, len
    MOV EAX, 3
    MOV EBX, fd
    MOV ECX, buf
    MOV EDX, len
    INT 0x80
.endm

.macro sys_open filename, flags, mode
    MOV EAX, 5
    MOV EBX, filename
    MOV ECX, flags
    MOV EDX, mode
    INT 0x80
.endm

.macro sys_close fd
    MOV EAX, 6
    MOV EBX, fd
    INT 0x80
.endm

.macro function name
.global name
name:
    PUSH EBP
    MOV EBP, ESP
.endm

.macro end_function
    MOV ESP, EBP
    POP EBP
    RET
.endm

.equ STDIN, 0
.equ STDOUT, 1
