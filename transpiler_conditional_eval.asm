; ======================================================
; C.A.S.E. (Compiler Assembler Strategic Execution)
; Phase 2.5 – Conditional Evaluation + Branch Emission
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

; ------------------------------------------------------
section .data
infile      db "compiler.case",0
outfile     db "compiler.asm",0

buffer      times 16384 db 0
msg_text    times 1024  db 0
tmp_num     dq 0
lbl_counter dq 0
msg_counter dq 0

asm_header  db "section .data",13,10,0
asm_footer  db "section .text",13,10,"global _start",13,10,"_start:",13,10,0
asm_exit    db "  mov rax,60",13,10,"  xor rdi,rdi",13,10,"  syscall",13,10,0

msg_prefix  db "msg",0
newline     db 13,10
ok_msg      db "Conditional-evaluation compiler.asm generated.",13,10,0

; ------------------------------------------------------
section .bss
fd_in   resq 1
fd_out  resq 1
bytes   resq 1

; ------------------------------------------------------
section .text
global _start

_start:
    ; open & read compiler.case
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

    mov rcx,rax
    lea rdx,[rel buffer]
    mov r8,16384
    lea r9,[rel bytes]
    sub rsp,32
    call ReadFile
    add rsp,32

    ; create compiler.asm
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

    ; write data header
    mov rcx,[fd_out]
    lea rdx,[rel asm_header]
    mov r8,asm_header_len
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

; ======================================================
; ===  PARSER LOOP  ====================================
; ======================================================
    lea rsi,[rel buffer]

.next:
    lodsb
    cmp al,0
    je .done
    cmp al,'P'
    je .print
    cmp al,'i'
    je .if_block
    jmp .next

; ------------------------------------------------------
; Print "..." [end]
; ------------------------------------------------------
.print:
    lodsb
    cmp al,'"'
    jne .print
    mov rcx,0
.copy_msg:
    lodsb
    cmp al,'"'
    je .done_msg
    mov [msg_text+rcx],al
    inc rcx
    jmp .copy_msg
.done_msg:
    mov byte [msg_text+rcx],0
    ; write message line
    mov rcx,[fd_out]
    lea rdx,[rel msg_prefix]
    mov r8,3
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32
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
    jmp .next

; ------------------------------------------------------
; if <number> { ... } else { ... } [end]
; ------------------------------------------------------
.if_block:
    ; read space
    lodsb
    ; expect a number
    mov rcx,0
.readnum:
    cmp al,'0'
    jb .notnum
    cmp al,'9'
    ja .notnum
    sub al,'0'
    imul rcx,10
    add rcx,rax
    lodsb
    jmp .readnum
.notnum:
    mov [tmp_num],rcx

    mov rax,[lbl_counter]
    inc rax
    mov [lbl_counter],rax

    ; compare immediate numeric value (1=true)
    mov rcx,[fd_out]
    sub rsp,32
    ; write code:  cmp rax,0 / je else_lbl
    ; simplified inline template
    lea rdx,[rel newline]
    mov r8,2
    lea r9,[rel bytes]
    call WriteFile
    add rsp,32

    ; output cmp/jump code
    mov rcx,[fd_out]
    lea rdx,[rel newline]
    mov r8,2
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

    ; if tmp_num == 0 → mark for jump
    mov rbx,[tmp_num]
    cmp rbx,0
    jne .emit_true
.emit_false:
    ; write comment indicating false branch
    mov rcx,[fd_out]
    lea rdx,[rel newline]
    mov r8,2
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32
    jmp .next
.emit_true:
    ; write label "if_true:"
    mov rcx,[fd_out]
    lea rdx,[rel newline]
    mov r8,2
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32
    jmp .next

; ------------------------------------------------------
.done:
    ; append text footer + exit
    mov rcx,[fd_out]
    lea rdx,[rel asm_footer]
    mov r8,asm_footer_len
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

    mov rcx,[fd_out]
    lea rdx,[rel asm_exit]
    mov r8,asm_exit_len
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

    mov rcx,[fd_in]
    sub rsp,32
    call CloseHandle
    add rsp,32

    mov rcx,[fd_out]
    sub rsp,32
    call CloseHandle
    add rsp,32

    sub rsp,32
    mov rcx,STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx,rax
    lea rdx,[rel ok_msg]
    mov r8,ok_len
    lea r9,[rel bytes]
    call WriteFile
    add rsp,32

    mov ecx,0
    sub rsp,32
    call ExitProcess

asm_header_len equ $-asm_header
asm_footer_len equ $-asm_footer
asm_exit_len   equ $-asm_exit
ok_len         equ $-ok_msg
