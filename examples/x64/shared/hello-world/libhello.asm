; libhello.asm — "Hello World" shared library
; Prints "Hi!\n" using register-based string construction
.section .text
.global say_hello
say_hello:
    ; Build "Hi!\n" on stack:
    ; "H" = 0x48, "i" = 0x69, "!" = 0x21, "\n" = 0x0a
    sub rsp, 8
    mov dword [rsp], 0x0a216948    ; "Hi!\n" in little-endian
    mov eax, 1
    mov edi, 1
    mov rsi, rsp
    mov edx, 4
    syscall
    add rsp, 8
    ret
