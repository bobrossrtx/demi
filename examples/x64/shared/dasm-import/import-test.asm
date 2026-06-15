; import-test.asm — DASM program importing a .so function
; Build:
;   demi -A import-test.asm --assembly-target x86-elf64 -ao import-test.o
;   gcc -no-pie -o import-test import-test.o -L../return-42 -l:libreturn42.so -Wl,-rpath,../return-42

.section .text
.global main
.extern greet
main:
    call greet
    mov eax, 0
    ret
