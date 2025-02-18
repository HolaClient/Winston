BITS 32

extern  _WSAStartup@8
extern  _WSACleanup@0
extern  _socket@12
extern  _bind@12
extern  _listen@8
extern  _accept@12
extern  _send@16
extern  _closesocket@4
extern  _ExitProcess@4
extern  _WriteConsoleA@20
extern  _GetStdHandle@4
extern  _shutdown@8
extern _CreateIoCompletionPort@16
extern _GetQueuedCompletionStatus@20
extern _CreateThread@24
extern _WSARecv@28
extern _setsockopt@20

section .data
sockaddr_in:
    dw 2
    dw 0x2008
    dd 0
    times 8 db 0

welcome_msg db "Hello from Windows server!", 0
msg_len     equ $ - welcome_msg

server_start_msg db "Windows server starting...", 0
server_start_msg_len equ $ - server_start_msg

bind_success_msg db "Server bound to port 2080, waiting for connections...", 13, 10, 0
bind_success_len equ $ - bind_success_msg

accept_msg db "Connection accepted from client", 13, 10, 0
accept_msg_len equ $ - accept_msg

http_response db "HTTP/1.1 200 OK", 13, 10
             db "Content-Type: text/plain", 13, 10
             db "Content-Length: 12", 13, 10
             db "Connection: close", 13, 10
             db 13, 10
             db "Hello World", 13, 10
http_response_len equ $ - http_response

INVALID_HANDLE_VALUE equ -1
SO_SNDBUF equ 0x1001
SO_RCVBUF equ 0x1002
TCP_NODELAY equ 0x0001
BUFFER_SIZE equ 4096
MAX_THREADS equ 32

socket_recv_buffer dd 0
socket_send_buffer dd 262144
tcp_nodelay dd 1

worker_error_msg db "Worker thread error", 13, 10, 0
worker_error_len equ $ - worker_error_msg

iocp_error_msg db "IOCP creation failed", 13, 10, 0
iocp_error_len equ $ - iocp_error_msg

section .bss
    chars_written    resd 1
    wsadata         resb 400
    console_handle  resd 1
    iocp_handle     resd 1
    thread_handles  resd MAX_THREADS
    overlapped      resb 20 * 1024
    buffer          resb BUFFER_SIZE * 1024
    lpNumberOfBytes resd 1
    lpCompletionKey resd 1
    lpOverlapped    resd 1

section .text
global _main
_main:
    push dword -11
    call _GetStdHandle@4
    mov [console_handle], eax
    
    push dword 0
    push chars_written
    push server_start_msg_len
    push server_start_msg
    push dword [console_handle]

    cmp dword [console_handle], -1
    je err

    call _WriteConsoleA@20
    jmp cont

err:
    push dword 0
    call _ExitProcess@4

cont:
    push wsadata
    push dword 0x202
    call _WSAStartup@8
    test eax, eax
    jnz fail

    push 0
    push 1
    push 2
    call _socket@12
    cmp eax, -1
    je fail
    mov ebx, eax

    push dword 0
    push ebx
    push dword 0
    push INVALID_HANDLE_VALUE
    call _CreateIoCompletionPort@16
    mov [iocp_handle], eax
    
    test eax, eax
    jz iocp_failed

    mov ecx, MAX_THREADS
create_threads:
    push ecx
    
    push dword 0
    push dword 0
    push dword [iocp_handle]
    push worker_thread
    push dword 0
    push dword 0
    call _CreateThread@24
    
    test eax, eax
    jz thread_failed
    
    pop ecx
    dec ecx
    jnz create_threads

    push dword 4
    push tcp_nodelay
    push dword TCP_NODELAY
    push dword 6
    push ebx
    call _setsockopt@20

    push 16
    push sockaddr_in
    push ebx
    call _bind@12
    cmp eax, -1
    je fail

    push dword 0
    push chars_written
    push bind_success_len
    push bind_success_msg
    push dword [console_handle]
    call _WriteConsoleA@20

    push 10
    push ebx
    call _listen@8
    cmp eax, -1
    je fail

loop:
    push 0
    push 0
    push ebx
    call _accept@12
    cmp eax, -1
    je fail
    mov esi, eax

    push dword 0
    push chars_written
    push accept_msg_len
    push accept_msg
    push dword [console_handle]
    call _WriteConsoleA@20

    push 0
    push http_response_len
    push http_response
    push esi
    call _send@16
    
    cmp eax, -1
    je close
    
    push 1
    push esi
    call _shutdown@8

close:
    push esi
    call _closesocket@4
    
    jmp loop

iocp_failed:
    push dword 0
    push chars_written
    push iocp_error_len
    push iocp_error_msg
    push dword [console_handle]
    call _WriteConsoleA@20
    jmp fail

thread_failed:
    push dword 0
    push chars_written
    push worker_error_len
    push worker_error_msg
    push dword [console_handle]
    call _WriteConsoleA@20
    jmp fail

fail:
    push ebx
    call _closesocket@4
    call _WSACleanup@0

    push 0
    call _ExitProcess@4

worker_thread:
    push ebp
    mov ebp, esp
    sub esp, 16

    mov ebx, [ebp + 8]

worker_loop:
    push dword 0
    lea eax, [lpOverlapped]
    push eax
    lea eax, [lpCompletionKey]
    push eax
    lea eax, [lpNumberOfBytes]
    push eax
    push ebx
    call _GetQueuedCompletionStatus@20
    
    test eax, eax
    jnz worker_loop
    
    mov esp, ebp
    pop ebp
    ret 4

    test eax, eax
    jz worker_thread
    
    mov esp, ebp
    pop ebp
    jmp worker_thread
