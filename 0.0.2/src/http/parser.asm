BITS 64
DEFAULT REL

%ifidn __OUTPUT_FORMAT__, win64
    %define WINDOWS
%elifidn __OUTPUT_FORMAT__, elf64
    %define LINUX
%endif

section .data
    align 16
    http_data:
        db 'HTTP/1.1 200 OK', 13, 10
        db 'Content-Type: application/json', 13, 10
        db 'Content-Length: 15', 13, 10
        db 'Connection: keep-alive', 13, 10
        db 13, 10
        db '{"status":"ok"}'
    resp_len equ $ - http_data

section .text
    global parse_req
    global send_resp
    extern send

%ifdef WINDOWS
    %define ARG1 rcx
    %define ARG2 rdx
    %define ARG3 r8
    %define ARG4 r9
    %define SHADOW_SPACE 32
%else
    %define ARG1 rdi
    %define ARG2 rsi
    %define ARG3 rdx
    %define ARG4 rcx
    %define SHADOW_SPACE 0
%endif

parse_req:
    push rbp
    mov rbp, rsp
%ifdef WINDOWS
    push rsi
    push rdi
    sub rsp, 32
    mov rsi, rdx
    mov edi, r8d
    test rsi, rsi
    jz .err_win
    test edi, edi
    jz .err_win
    mov eax, dword [rsi]
%else
    sub rsp, SHADOW_SPACE
    test ARG2, ARG2
    jz .err
    test ARG3, ARG3
    jz .err
    mov eax, dword [ARG2]
%endif

    cmp eax, 0x20544547
    sete al
    movzx eax, al

%ifdef WINDOWS
    lea rsp, [rbp-16]
    pop rdi
    pop rsi
    pop rbp
    ret

.err_win:
    xor eax, eax
    lea rsp, [rbp-16]
    pop rdi
    pop rsi
    pop rbp
    ret
%else
    leave
    ret

.err:
    xor eax, eax
    leave
    ret
%endif

send_resp:
    push rbp
    mov rbp, rsp
%ifdef WINDOWS
    push rsi
    push rdi
    sub rsp, 32
    mov rax, rcx
    lea rdx, [rel http_data]
    mov r8, resp_len
    xor r9d, r9d
    mov rcx, rax
%else
    push r12
    mov r12, rdi
    mov rdi, r12
    lea rsi, [rel http_data]
    mov rdx, resp_len
    xor rcx, rcx
%endif

    call send
    
%ifdef WINDOWS
    lea rsp, [rbp-16]
    pop rdi
    pop rsi
%else
    pop r12
%endif
    pop rbp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
