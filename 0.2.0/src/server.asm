BITS 64
DEFAULT REL

%define AF_INET     2
%define SOCK_STREAM 1
%define SOL_SOCKET  1
%define SO_REUSEADDR 2
%define SOMAXCONN   128

section .data
    sock_addr:
        dw AF_INET
        dw 0
        dd 0
        dq 0
    
    opt_val:
        dd 1
        dd 0

section .text
    global srv_init
    global srv_listen
    global srv_stop

srv_init:
    push rbp
    mov rbp, rsp
    
    mov rax, 41
    mov rdi, AF_INET
    mov rsi, SOCK_STREAM
    xor rdx, rdx
    syscall
    
    cmp rax, 0
    jl .error
    
    push rax
    
    mov rdi, rax
    mov rax, 54
    mov rsi, SOL_SOCKET
    mov rdx, SO_REUSEADDR
    lea r10, [opt_val]
    mov r8, 4
    syscall
    
    pop rax
    
    mov rsp, rbp
    pop rbp
    ret

.error:
    xor rax, rax
    mov rsp, rbp
    pop rbp
    ret

srv_listen:
    push rbp
    mov rbp, rsp
    push rbx
    mov rbx, rdi
    
    mov ax, si
    xchg ah, al
    mov [sock_addr + 2], ax
    
    mov rax, 49
    mov rdi, rbx
    lea rsi, [sock_addr]
    mov rdx, 16
    syscall
    
    mov rax, 50
    mov rdi, rbx
    mov rsi, SOMAXCONN
    syscall
    
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

srv_stop:
    push rbp
    mov rbp, rsp
    
    mov rax, 3
    syscall
    
    mov rsp, rbp
    pop rbp
    ret