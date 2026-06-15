; ============================================================
; main.asm — DASM program that calls a shared library function
; ============================================================
; Build: demi -A main.asm --assembly-target x86-elf64 -ao main.o
; Link:  gcc -o main main.o -L. -lgreet -Wl,-rpath,.
; Run:   ./main
; Output:
;   About to call shared library...
;   Hello from shared library!
;   Back from shared library. Done.
; ============================================================

.section rodata
before_msg:
    .string "About to call shared library...\n"
after_msg:
    .string "Back from shared library. Done.\n"

.section .text
.global main
main:
    ; Print "About to call..."
    mov eax, 1
    mov edi, 1
    mov rsi, before_msg
    mov edx, 33
    syscall

    ; Call shared library function
    call greet

    ; Print "Back from shared library..."
    mov eax, 1
    mov edi, 1
    mov rsi, after_msg
    mov edx, 31
    syscall

    ; exit(0)
    mov eax, 60
    xor edi, edi
    syscall

; Declare external symbol from shared library
.extern greet
