; ==========================================
; Simple System Write (x86 32-bit)
; ==========================================
; Simple test - write to stdout to verify syscalls work
; Uses 32-bit registers

_start:
    ; sys_write: write to stdout
    LOAD_IMM EAX, 4         ; sys_write
    LOAD_IMM EBX, 1         ; stdout (fd=1)
    LOAD_IMM ECX, msg       ; buffer address
    LOAD_IMM EDX, 14        ; count
    INT 0x80
    
    ; sys_exit
    LOAD_IMM EAX, 1         ; sys_exit
    LOAD_IMM EBX, 0         ; exit code 0
    INT 0x80
    
    HALT

msg:
    DB "Hello, World!", 10
