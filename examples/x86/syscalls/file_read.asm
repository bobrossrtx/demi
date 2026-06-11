; ==========================================
; File Read Test (x86 32-bit)
; ==========================================
; Opens /tmp/demi_input.txt and reads its contents
; Uses 32-bit registers with .data and .text sections

.data
    filename: DB "/etc/hostname", 0
    missing_msg: DB "file_read: could not open /etc/hostname", 10, 0
    buffer: RESB 50

.text
_start:
    ; sys_open - open the file for reading
    LOAD_IMM EAX, 5         ; sys_open (syscall 5)
    LOAD_IMM EBX, filename  ; filename address
    LOAD_IMM ECX, 0         ; O_RDONLY (0)
    LOAD_IMM EDX, 0         ; mode (not used for read)
    INT 0x80                ; Call kernel - returns fd in EAX

    CMP EAX, 0
    JL open_failed
    
    ; Save file descriptor
    MOV ESI, EAX            ; ESI = fd
    
    ; sys_read - read file contents into buffer
    LOAD_IMM EAX, 3         ; sys_read (syscall 3)
    MOV EBX, ESI            ; fd
    LOAD_IMM ECX, buffer    ; buffer address
    LOAD_IMM EDX, 50        ; max bytes to read
    INT 0x80                ; Call kernel - returns bytes read in EAX
    
    ; Save bytes read
    MOV EDI, EAX            ; EDI = bytes_read
    
    ; sys_write - write to stdout
    LOAD_IMM EAX, 4         ; sys_write (syscall 4)
    LOAD_IMM EBX, 1         ; stdout
    LOAD_IMM ECX, buffer    ; buffer (same as read buffer)
    MOV EDX, EDI            ; count = bytes_read
    INT 0x80                ; Call kernel
    
    ; sys_close
    LOAD_IMM EAX, 6         ; sys_close (syscall 6)
    MOV EBX, ESI            ; fd
    INT 0x80                ; Call kernel
    
    ; sys_exit
    LOAD_IMM EAX, 1         ; sys_exit
    LOAD_IMM EBX, 0         ; exit code 0
    INT 0x80
    
    HALT

open_failed:
    LOAD_IMM EAX, 4
    LOAD_IMM EBX, 1
    LOAD_IMM ECX, missing_msg
    LOAD_IMM EDX, 40
    INT 0x80

    LOAD_IMM EAX, 1
    LOAD_IMM EBX, 1
    INT 0x80
    HALT
