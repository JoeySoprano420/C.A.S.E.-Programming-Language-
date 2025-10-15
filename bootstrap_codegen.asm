; ======================================================
;  bootstrap_codegen.asm — C.A.S.E. Backend Executor
;  (Windows x86-64 PE)
; ======================================================

extern  CreateFileA, ReadFile, WriteFile, CloseHandle, ExitProcess
extern  GetStdHandle

STD_OUTPUT_HANDLE       equ -11
GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h
CREATE_ALWAYS           equ 2
OPEN_EXISTING           equ 3
FILE_SHARE_READ         equ 1
FILE_ATTRIBUTE_NORMAL   equ 80h

section .data
infile      db "input.case",0
outfile     db "output.asm",0
buffer      times 32768 db 0
msg_text    times 2048 db 0
hdr         db "section .data",13,10,0
ftr         db "section .text",13,10,"global _start",13,10,"_start:",13,10,0
exitseq     db "  mov rax,60",13,10,"  xor rdi,rdi",13,10,"  syscall",13,10,0
okmsg       db "output.asm generated",13,10,0
quote       db '"'
newline     db 13,10

section .bss
fd_in   resq 1
fd_out  resq 1
bytes   resq 1

section .text
global _start

; ------------------------------------------------------
_start:
    ; open input.case
    sub rsp,32
    lea rcx,[rel infile]
    mov rdx,GENERIC_READ
    mov r8,FILE_SHARE_READ
    mov r9,0
    push FILE_ATTRIBUTE_NORMAL
    push OPEN_EXISTING
    push 0
    call CreateFileA
    add rsp,32+8*3
    mov [fd_in],rax

    ; read contents
    mov rcx,rax
    lea rdx,[rel buffer]
    mov r8,32768
    lea r9,[rel bytes]
    sub rsp,32
    call ReadFile
    add rsp,32

    ; create output.asm
    sub rsp,32
    lea rcx,[rel outfile]
    mov rdx,GENERIC_WRITE
    mov r8,0
    mov r9,0
    push FILE_ATTRIBUTE_NORMAL
    push CREATE_ALWAYS
    push 0
    call CreateFileA
    add rsp,32+8*3
    mov [fd_out],rax

    ; emit header
    mov rcx,[fd_out]
    lea rdx,[rel hdr]
    mov r8,hdrlen
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

    ; --------------------------------------------------
    ;  PARSE LOOP — find Print "..." [end]
    ; --------------------------------------------------
    lea rsi,[rel buffer]
.nextch:
    lodsb
    cmp al,0
    je .done
    cmp al,'P'
    jne .nextch
    lodsb
    cmp al,'r'
    jne .nextch
    lodsb
    cmp al,'i'
    jne .nextch
    lodsb
    cmp al,'n'
    jne .nextch
    lodsb
    cmp al,'t'
    jne .nextch

.findquote:
    lodsb
    cmp al,'"'
    jne .findquote
    mov rcx,0
.copy:
    lodsb
    cmp al,'"'
    je .emit
    mov [msg_text+rcx],al
    inc rcx
    jmp .copy
.emit:
    mov byte [msg_text+rcx],0
    ; write msg line
    mov rcx,[fd_out]
    lea rdx,[rel msg_text]
    mov r8,rcx
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32
    mov rcx,[fd_out]
    lea rdx,[rel newline]
    mov r8,2
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32
    jmp .nextch

.done:
    ; footer + exit sequence
    mov rcx,[fd_out]
    lea rdx,[rel ftr]
    mov r8,ftrlen
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

    mov rcx,[fd_out]
    lea rdx,[rel exitseq]
    mov r8,exitlen
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

    ; close handles
    mov rcx,[fd_in]
    sub rsp,32
    call CloseHandle
    add rsp,32
    mov rcx,[fd_out]
    sub rsp,32
    call CloseHandle
    add rsp,32

    ; notify
    sub rsp,32
    mov rcx,STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx,rax
    lea rdx,[rel okmsg]
    mov r8,oklen
    lea r9,[rel bytes]
    call WriteFile
    add rsp,32

    mov ecx,0
    sub rsp,32
    call ExitProcess

; ------------------------------------------------------
hdrlen  equ $-hdr
ftrlen  equ $-ftr
exitlen equ $-exitseq
oklen   equ $-okmsg
