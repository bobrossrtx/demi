.section .data
pkt:
    .byte 0x08, 0x00, 0x2D, 0x3C, 0x12, 0x34, 0x00, 0x01
    .byte 0x44, 0x41, 0x53, 0x4D, 0x21, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.section .bss
reply:
    .space 100
.section .data
sockaddr:
    .byte 0x02, 0x00, 0x00, 0x00
    .byte 8, 8, 8, 8
    .space 8
timeval:
    .byte 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
msg_header:
    .string "DASM ping to 8.8.8.8\n"
msg_from:
    .string "64 bytes from "
msg_dot:
    .string "."
msg_seq:
    .string ": icmp_seq="
msg_ttl:
    .string " ttl="
msg_time:
    .string " ms\n"
msg_err:
    .string "ERR\n"

.section .text
.global _start
_start:
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_header
    mov rdx, 22
    syscall

    mov rax, 41
    mov rdi, 2
    mov rsi, 3
    mov rdx, 1
    syscall
    cmp rax, 0
    jl error_exit
    mov r12, rax

    mov rax, 54
    mov rdi, r12
    mov rsi, 1
    mov rdx, 20
    mov r10, timeval
    mov r8, 16
    syscall

    mov rax, 44
    mov rdi, r12
    mov rsi, pkt
    mov rdx, 64
    xor r10, r10
    mov r8, sockaddr
    mov r9, 16
    syscall
    cmp rax, 0
    jl error_exit

    mov rax, 45
    mov rdi, r12
    mov rsi, reply
    mov rdx, 100
    xor r10, r10
    xor r8, r8
    xor r9, r9
    syscall
    cmp rax, 0
    jl error_exit

    mov rax, 1
    mov rdi, 1
    mov rsi, msg_from
    mov rdx, 14
    syscall

    mov rsi, reply
    add rsi, 12
    call print_ip

    mov rax, 1
    mov rdi, 1
    mov rsi, msg_seq
    mov rdx, 11
    syscall

    mov rsi, reply
    add rsi, 27
    lodsb
    call putdec

    mov rax, 1
    mov rdi, 1
    mov rsi, msg_ttl
    mov rdx, 4
    syscall

    mov rsi, reply
    add rsi, 8
    lodsb
    call putdec

    mov rax, 1
    mov rdi, 1
    mov rsi, msg_time
    mov rdx, 4
    syscall

    mov rax, 3
    mov rdi, r12
    syscall
    mov rax, 60
    xor rdi, rdi
    syscall

print_ip:
    push rbx
    mov rbx, rsi
    lodsb
    push rbx
    call putdec
    pop rbx
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_dot
    mov rdx, 1
    syscall
    mov rsi, rbx
    add rsi, 1
    lodsb
    push rbx
    call putdec
    pop rbx
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_dot
    mov rdx, 1
    syscall
    mov rsi, rbx
    add rsi, 2
    lodsb
    push rbx
    call putdec
    pop rbx
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_dot
    mov rdx, 1
    syscall
    mov rsi, rbx
    add rsi, 3
    lodsb
    call putdec
    pop rbx
    ret

putdec:
    push rbx
    push rsi
    cmp al, 200
    jb pd_try100
    sub al, 200
    push rax
    mov al, 50
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
    jmp pd_tens
pd_try100:
    cmp al, 100
    jb pd_tens
    sub al, 100
    push rax
    mov al, 49
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
pd_tens:
    cmp al, 90
    jb pd_t80
    sub al, 90
    push rax
    mov al, 57
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
    jmp pd_ones
pd_t80:
    cmp al, 80
    jb pd_t70
    sub al, 80
    push rax
    mov al, 56
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
    jmp pd_ones
pd_t70:
    cmp al, 70
    jb pd_t60
    sub al, 70
    push rax
    mov al, 55
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
    jmp pd_ones
pd_t60:
    cmp al, 60
    jb pd_t50
    sub al, 60
    push rax
    mov al, 54
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
    jmp pd_ones
pd_t50:
    cmp al, 50
    jb pd_t40
    sub al, 50
    push rax
    mov al, 53
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
    jmp pd_ones
pd_t40:
    cmp al, 40
    jb pd_t30
    sub al, 40
    push rax
    mov al, 52
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
    jmp pd_ones
pd_t30:
    cmp al, 30
    jb pd_t20
    sub al, 30
    push rax
    mov al, 51
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
    jmp pd_ones
pd_t20:
    cmp al, 20
    jb pd_t10
    sub al, 20
    push rax
    mov al, 50
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
    jmp pd_ones
pd_t10:
    cmp al, 10
    jb pd_ones
    sub al, 10
    push rax
    mov al, 49
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rax
pd_ones:
    add al, 48
    push rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall
    pop rax
    pop rsi
    pop rbx
    ret

error_exit:
    mov rax, 1
    mov rdi, 2
    mov rsi, msg_err
    mov rdx, 4
    syscall
    mov rax, 60
    mov rdi, 1
    syscall
