.section .data
sockaddr:
    .byte 0x02, 0x00             ; AF_INET
    .byte 0x23, 0x82             ; port 9090 (network byte order)
    .byte 0, 0, 0, 0             ; INADDR_ANY
    .byte 0,0,0,0,0,0,0,0        ; padding
client_addr:
    .space 16
client_addr_len:
    .byte 16,0,0,0,0,0,0,0
buf:
    .space 1024
msg_listen:
    .string "Listening on port 9090\n"
msg_conn:
    .string "Client connected\n"
msg_echo:
    .string "Echo: "
msg_newline:
    .string "\n"
msg_err:
    .string "Error\n"

.section .text
.global _start
_start:
    ; socket(AF_INET=2, SOCK_STREAM=1, 0)
    mov rax, 41
    mov rdi, 2
    mov rsi, 1
    xor rdx, rdx
    syscall
    cmp rax, 0
    jl error_exit
    mov r12, rax            ; server_fd in r12

    ; bind(server_fd, &sockaddr, 16)
    mov rax, 49
    mov rdi, r12
    mov rsi, sockaddr
    mov rdx, 16
    syscall
    cmp rax, 0
    jl error_exit

    ; listen(server_fd, 1)
    mov rax, 50
    mov rdi, r12
    mov rsi, 1
    syscall
    cmp rax, 0
    jl error_exit

    ; Print listening message
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_listen
    mov rdx, 23
    syscall

accept_loop:
    ; accept(server_fd, &client_addr, &client_addr_len)
    mov rax, 43
    mov rdi, r12
    mov rsi, client_addr
    mov rdx, client_addr_len
    syscall
    cmp rax, 0
    jl error_exit
    mov r13, rax            ; client_fd in r13

    ; Print connection message
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_conn
    mov rdx, 18
    syscall

echo_loop:
    ; recv(client_fd, buf, 1024, 0)
    mov rax, 45
    mov rdi, r13
    mov rsi, buf
    mov rdx, 1024
    xor r10, r10
    xor r8, r8
    xor r9, r9
    syscall
    cmp rax, 0
    jle close_client

    ; Save byte count
    mov r14, rax

    ; Print "Echo: "
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_echo
    mov rdx, 6
    syscall

    ; send(client_fd, buf, count, 0)
    mov rax, 44
    mov rdi, r13
    mov rsi, buf
    mov rdx, r14
    xor r10, r10
    xor r8, r8
    xor r9, r9
    syscall
    cmp rax, 0
    jl error_exit

    ; Print received data
    mov rax, 1
    mov rdi, 1
    mov rsi, buf
    mov rdx, r14
    syscall

    ; Print newline
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_newline
    mov rdx, 1
    syscall

    jmp echo_loop

close_client:
    ; close(client_fd)
    mov rax, 3
    mov rdi, r13
    syscall
    jmp accept_loop

error_exit:
    mov rax, 1
    mov rdi, 2
    mov rsi, msg_err
    mov rdx, 6
    syscall
    mov rax, 60
    mov rdi, 1
    syscall
