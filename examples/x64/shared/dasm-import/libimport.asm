; libimport.asm — DASM shared library, prints from .so
.section .text
.global say_hello
say_hello:
    sub rsp, 16
    mov dword [rsp],    0x66206948    ; "Hi f"
    mov dword [rsp+4],  0x206d6f72    ; "rom "
    mov dword [rsp+8],  0x0a62696c    ; "lib\n"
    mov eax, 1
    mov edi, 1
    mov rsi, rsp
    mov edx, 12
    syscall
    add rsp, 16
    ret

.global compute
compute:
    mov eax, edi
    add eax, esi
    ret
