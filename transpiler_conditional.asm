; =====================================================
; C.A.S.E. (Compiler Assembler Strategic Execution)
; Phase 2 Bootstrap Transpiler — Conditional + Multi-Fn
;
; Input : compiler.case
; Output: compiler.asm
;
; Assemble : nasm -f win64 transpiler_conditional.asm -o transpiler.obj
; Link     : clang transpiler.obj -o transpiler.exe -Wl,/SUBSYSTEM:CONSOLE,/ENTRY:_start
; Run      : transpiler.exe
; =====================================================

extern  CreateFileA, ReadFile, WriteFile, CloseHandle, ExitProcess
extern  GetStdHandle

STD_OUTPUT_HANDLE       equ -11
GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h
CREATE_ALWAYS           equ 2
OPEN_EXISTING           equ 3
FILE_SHARE_READ         equ 1
FILE_ATTRIBUTE_NORMAL   equ 80h

; ----------------------------------------------------
section .data
infile      db "compiler.case",0
outfile     db "compiler.asm",0

buffer      times 16384 db 0
msg_text    times 1024  db 0
tok         times 64    db 0
fn_counter  dq 0
msg_counter dq 0
lbl_counter dq 0

; ---- Assembly Templates ----
asm_header  db "section .data",13,10,0
asm_footer  db "section .text",13,10,"global _start",13,10,0
asm_exit    db "  mov rax,60",13,10,"  xor rdi,rdi",13,10,"  syscall",13,10,0

if_prefix   db "if_",0
else_prefix db "else_",0
end_prefix  db "endif_",0
msg_prefix  db "msg",0
newline     db 13,10

ok_msg db "Conditional compiler.asm generated successfully.",13,10,0

; ----------------------------------------------------
section .bss
fd_in   resq 1
fd_out  resq 1
bytes   resq 1

; ----------------------------------------------------
section .text
global _start

; ====================================================
; ========== ENTRY ===================================
; ====================================================
_start:
    ; open .case for read
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

    ; read file
    mov rcx,[fd_in]
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

    ; write header
    mov rcx,[fd_out]
    lea rdx,[rel asm_header]
    mov r8,asm_header_len
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

; ====================================================
; ========== PARSE LOOP ===============================
; ====================================================
    lea rsi,[rel buffer]

.next:
    lodsb
    cmp al,0
    je .done
    cmp al,'F'
    je .fn_detect
    cmp al,'P'
    je .print_detect
    cmp al,'i'
    je .if_detect
    jmp .next

; ----------------------------------------------------
;  Fn handler
; ----------------------------------------------------
.fn_detect:
    ; increment function counter
    mov rax,[fn_counter]
    inc rax
    mov [fn_counter],rax

    ; write .text footer (only once for first fn)
    cmp rax,1
    jne .fn_label
    mov rcx,[fd_out]
    lea rdx,[rel asm_footer]
    mov r8,asm_footer_len
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

.fn_label:
    mov rcx,[fd_out]
    lea rdx,[rel newline]
    mov r8,2
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32
    jmp .next

; ----------------------------------------------------
;  Print handler
; ----------------------------------------------------
.print_detect:
    ; scan to first quote
.scan_q:
    lodsb
    cmp al,'"'
    jne .scan_q
    mov rcx,0
.copy_q:
    lodsb
    cmp al,'"'
    je .done_q
    mov [msg_text+rcx],al
    inc rcx
    jmp .copy_q
.done_q:
    mov byte [msg_text+rcx],0

    ; increment msg id
    mov rax,[msg_counter]
    inc rax
    mov [msg_counter],rax

    ; write message db line
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

; ----------------------------------------------------
;  if ... else ... [end]
; ----------------------------------------------------
.if_detect:
    ; increment label counter
    mov rax,[lbl_counter]
    inc rax
    mov [lbl_counter],rax

    ; Write "if_" label start
    mov rcx,[fd_out]
    lea rdx,[rel if_prefix]
    mov r8,3
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

; ----------------------------------------------------
.done:
    ; append exit sequence
    mov rcx,[fd_out]
    lea rdx,[rel asm_exit]
    mov r8,asm_exit_len
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

    ; notify success
    sub rsp,32
    mov rcx,STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx,rax
    lea rdx,[rel ok_msg]
    mov r8,ok_len
    lea r9,[rel bytes]
    call WriteFile
    add rsp,32

    ; exit
    mov ecx,0
    sub rsp,32
    call ExitProcess

; ====================================================
asm_header_len equ $-asm_header
asm_footer_len equ $-asm_footer
asm_exit_len   equ $-asm_exit
ok_len         equ $-ok_msg
