; ==========================================
; File Write Test (x86 32-bit)
; ==========================================
; Creates /tmp/demi_test.txt and writes to it
; Uses 32-bit registers with .data and .text sections

.data
    filename: DB "/tmp/demi_test.txt", 0
    message: DB "Hello from DemiEngine!", 10, 0
    success_msg: DB "File written successfully!", 10, 0

.text
_start:
    ; sys_open: open file for writing (create if doesn't exist)
    LOAD_IMM EAX, 5         ; sys_open (syscall number 5)
    LOAD_IMM EBX, filename  ; pathname address
    LOAD_IMM ECX, 65        ; flags: O_WRONLY | O_CREAT = 0x41
    LOAD_IMM EDX, 420       ; mode: 0644 (octal 0644 = 420 decimal) - now works with 32-bit!
    INT 0x80                ; Real syscall to kernel
    
    ; EAX now contains file descriptor
    MOV EDI, EAX            ; Save fd to EDI
    
    ; sys_write: write message to file
    LOAD_IMM EAX, 4         ; sys_write (syscall number 4)
    MOV EBX, EDI            ; fd (from EDI)
    LOAD_IMM ECX, message   ; buffer address
    LOAD_IMM EDX, 23        ; count
    INT 0x80                ; Real syscall to kernel
    
    ; sys_close: close the file
    LOAD_IMM EAX, 6         ; sys_close (syscall number 6)
    MOV EBX, EDI            ; fd
    INT 0x80                ; Real syscall to kernel
    
    ; sys_write: print success message to stdout
    LOAD_IMM EAX, 4         ; sys_write
    LOAD_IMM EBX, 1         ; stdout
    LOAD_IMM ECX, success_msg ; success message address
    LOAD_IMM EDX, 27        ; count
    INT 0x80
    
    ; sys_exit
    LOAD_IMM EAX, 1         ; sys_exit (syscall number 1)
    LOAD_IMM EBX, 0         ; exit code 0
    INT 0x80                ; Stop VM
    
    HALT
