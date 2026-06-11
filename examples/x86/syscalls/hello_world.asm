; ==========================================
; System Call: Hello World (x86 32-bit)
; ==========================================
; Hello World using Linux syscall conventions (INT 0x80)
; Uses 32-bit registers following x86 calling convention

.org 0x00

_start:
    ; sys_write(stdout, message, length)
    LOAD_IMM EAX, 4         ; syscall 4 = sys_write
    LOAD_IMM EBX, 1         ; fd = 1 (stdout)
    LOAD_IMM ECX, msg       ; buffer address
    LOAD_IMM EDX, 49        ; message length
    INT 0x80                ; Call kernel
    
    ; sys_exit(0)
    LOAD_IMM EAX, 1         ; syscall 1 = sys_exit
    LOAD_IMM EBX, 0         ; exit code = 0
    INT 0x80                ; Call kernel
    
    HALT                    ; Should not reach here

msg:
    DB "syscalls_hello_world: sys_write -> Hello, World!", 10  ; Message with newline
