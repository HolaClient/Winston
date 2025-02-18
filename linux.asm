section .data
    PORT            equ 2080    
    BACKLOG         equ 10
    BUFFER_SIZE     equ 4096
    AF_INET         equ 2
    SOCK_STREAM     equ 1
    SOL_SOCKET      equ 1
    SO_REUSEADDR    equ 2
    MAX_THREADS     equ 32
    TCP_NODELAY     equ 1
    EPOLL_CTL_ADD   equ 1
    EPOLL_CTL_DEL   equ 2
    EPOLLIN         equ 1
    EPOLLOUT        equ 4
    EPOLLET         equ 1<<31
    
    sockopt         dd 1
    sockopt_len     equ 4

    dbg_port        db "Setting up port: %d", 10, 0
    err_msg         db "Error number: %d", 10, 0
    dbg_bound       db "Bound successfully", 10, 0
    dbg_bound_len   equ $ - dbg_bound
    dbg_acc         db "Accepted a connection", 10, 0
    dbg_acc_len     equ $ - dbg_acc

    srv_addr:
        s_family    dw AF_INET
        s_port      dw 0x0820   
        s_addr      dd 0        
        s_zero      times 8 db 0
    
    sock_err        db "Failed to create socket", 10
    sock_err_len    equ $ - sock_err
    bind_err        db "Failed to bind", 10
    bind_err_len    equ $ - bind_err
    lst_err         db "Failed to listen", 10
    lst_err_len     equ $ - lst_err
    succ_msg        db "Server started on port 2080", 10    
    succ_msg_len    equ $ - succ_msg
    
    http_ok         db 'HTTP/1.1 200 OK', 13, 10
    http_ok_len     equ $ - http_ok
    cont_type       db 'Content-Type: text/plain', 13, 10
    cont_len        equ $ - cont_type
    hdrs_end        db 13, 10
    hdrs_end_len    equ $ - hdrs_end
    resp_body       db 'Hello World', 13, 10
    resp_len        equ $ - resp_body

    start_msg       db "Linux server starting...", 10
    start_msg_len   equ $ - start_msg
    thread_err      db "Failed to create thread", 10
    thread_err_len  equ $ - thread_err
    epoll_err       db "Failed to create epoll", 10
    epoll_err_len   equ $ - epoll_err
    
    tcp_nodelay_val dd 1
    socket_buffer   dd 262144

section .bss
    sock_fd         resd 1
    cli_fd          resd 1
    buffer          resb BUFFER_SIZE
    cli_addr        resb 16
    cli_len         resd 1
    epoll_fd        resd 1
    thread_ids      resd MAX_THREADS
    epoll_events    resb 32 * 12

section .text
    global _start

create_thread:
    push ebp
    mov ebp, esp
    
    push 0
    push 0
    push worker_thread
    push 0
    mov eax, 120
    int 0x80
    
    mov esp, ebp
    pop ebp
    ret

worker_thread:
    push ebp
    mov ebp, esp
    
.loop:
    mov eax, 232
    mov ebx, [epoll_fd]
    mov ecx, epoll_events
    mov edx, 32
    mov esi, -1
    int 0x80
    
    test eax, eax
    jg .process_events
    jmp .loop

.process_events:
    jmp .loop

_start:
    mov eax, 4
    mov ebx, 1
    mov ecx, start_msg
    mov edx, start_msg_len
    int 0x80

    mov dword [cli_len], 16

    mov eax, 213
    mov ebx, 256
    int 0x80
    mov [epoll_fd], eax
    
    test eax, eax
    js epoll_error

    mov eax, 41          
    mov ebx, AF_INET     
    mov ecx, SOCK_STREAM 
    xor edx, edx        
    int 0x80
    
    cmp eax, 0
    jl sock_err_h
    mov [sock_fd], eax

    mov eax, 54
    mov ebx, [sock_fd]
    mov ecx, 6
    mov edx, TCP_NODELAY
    mov esi, tcp_nodelay_val
    mov edi, 4
    int 0x80

    mov eax, 54
    mov ebx, [sock_fd]
    mov ecx, SOL_SOCKET
    mov edx, SO_SNDBUF
    mov esi, socket_buffer
    mov edi, 4
    int 0x80

    mov eax, 54         
    mov ebx, [sock_fd]
    mov ecx, SOL_SOCKET
    mov edx, SO_REUSEADDR
    push dword 1        
    mov esi, esp
    push dword 4        
    mov edi, esp
    int 0x80
    add esp, 8         

    mov eax, 4
    mov ebx, 1
    mov ecx, dbg_bound   
    mov edx, dbg_bound_len
    int 0x80

    mov eax, 49         
    mov ebx, [sock_fd]
    lea ecx, [srv_addr]
    mov edx, 16         
    int 0x80

    push eax
    mov eax, 4
    mov ebx, 1
    mov ecx, err_msg         
    mov edx, 13
    int 0x80

    pop eax
    
    test eax, eax
    jnz bind_err_h     

    mov eax, 4
    mov ebx, 1
    mov ecx, dbg_bound
    mov edx, dbg_bound_len
    int 0x80

    mov eax, 50               
    mov ebx, [sock_fd]
    mov ecx, BACKLOG
    int 0x80

    mov ecx, MAX_THREADS
create_threads:
    push ecx
    call create_thread
    test eax, eax
    js thread_error
    pop ecx
    loop create_threads

    push dword [sock_fd]
    push dword EPOLLIN
    push dword [epoll_fd]
    call add_to_epoll

acc_loop:
    mov eax, 43               
    mov ebx, [sock_fd]
    mov ecx, cli_addr
    mov edx, cli_len
    int 0x80
    mov [cli_fd], eax

    mov eax, 4
    mov ebx, 1
    mov ecx, dbg_acc
    mov edx, dbg_acc_len
    int 0x80

    push eax                   
    
    mov eax, 4                
    mov ebx, [cli_fd]
    mov ecx, http_ok
    mov edx, http_ok_len
    int 0x80

    mov eax, 4
    mov ebx, [cli_fd]
    mov ecx, cont_type
    mov edx, cont_len
    int 0x80

    mov eax, 4
    mov ebx, [cli_fd]
    mov ecx, hdrs_end
    mov edx, hdrs_end_len
    int 0x80

    mov eax, 4
    mov ebx, [cli_fd]
    mov ecx, resp_body
    mov edx, resp_len
    int 0x80

    mov eax, 6                
    mov ebx, [cli_fd]
    int 0x80

    jmp acc_loop           

exit:
    mov eax, 6               
    mov ebx, [sock_fd]
    int 0x80

    mov eax, 1              
    xor ebx, ebx            
    int 0x80

sock_err_h:
    mov eax, 4         
    mov ebx, 2         
    mov ecx, sock_err
    mov edx, sock_err_len
    int 0x80
    jmp exit

bind_err_h:
    neg eax            
    push eax          
    
    mov eax, 4
    mov ebx, 2
    mov ecx, bind_err
    mov edx, bind_err_len
    int 0x80

    mov eax, 4
    mov ebx, 2
    mov ecx, err_msg
    mov edx, 13
    int 0x80
    
    pop eax           
    jmp exit

thread_error:
    mov eax, 4
    mov ebx, 2
    mov ecx, thread_err
    mov edx, thread_err_len
    int 0x80
    jmp exit

epoll_error:
    mov eax, 4
    mov ebx, 2
    mov ecx, epoll_err
    mov edx, epoll_err_len
    int 0x80
    jmp exit

add_to_epoll:
    push ebp
    mov ebp, esp
    
    mov eax, 233
    mov ebx, [ebp+12]
    mov ecx, EPOLL_CTL_ADD
    mov edx, [ebp+4]
    lea esi, [ebp+8]
    int 0x80
    
    mov esp, ebp
    pop ebp
    ret 12
