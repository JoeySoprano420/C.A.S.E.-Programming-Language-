; =====================================================
; C.A.S.E. (Compiler Assembler Strategic Execution)
; Phase 1.5 – Structured Transpiler (Windows PE)
;
; Supports:
;   - Fn main () ( ... ) [end]
;   - Print "..." [end]
;   - ret <value>
;
; Input : compiler.case
; Output: compiler.asm
;
; Assemble : nasm -f win64 transpiler_structured.asm -o transpiler.obj
; Link     : clang transpiler.obj -o transpiler.exe -Wl,/SUBSYSTEM:CONSOLE,/ENTRY:_start
; =====================================================

extern  CreateFileA, ReadFile, WriteFile, CloseHandle, ExitProcess
extern  GetStdHandle

STD_OUTPUT_HANDLE equ -11
GENERIC_READ      equ 80000000h
GENERIC_WRITE     equ 40000000h
CREATE_ALWAYS     equ 2
OPEN_EXISTING     equ 3
FILE_SHARE_READ   equ 1
FILE_ATTRIBUTE_NORMAL equ 80h

; ----------------------------------------------------
section .data
infile      db "compiler.case",0
outfile     db "compiler.asm",0

buffer      times 8192 db 0
msg_text    times 1024 db 0
print_count dq 0

; ---- Assembly Templates ----
asm_prologue db "section .data",13,10,0
asm_epilogue db "msglen equ $-msg",13,10,"section .text",13,10,"global _start",13,10,"_start:",13,10,0
asm_exitcode db "  mov rax,60",13,10,"  xor rdi,rdi",13,10,"  syscall",13,10,0

; ---- Fixed NASM Print Routine Template ----
print_template db "print_",0
print_syscall db ":",13,10,"  mov rax,1",13,10,"  mov rdi,1",13,10,"  lea rsi,[rel msg",0
print_suffix   db "]",13,10,"  mov rdx,msglen",13,10,"  syscall",13,10,"  ret",13,10,0

msg_prefix db "msg",0
quote db '"'
newline db 13,10

ok_msg db "Structured compiler.asm generated successfully.",13,10,0

; ----------------------------------------------------
section .bss
fd_in resq 1
fd_out resq 1
bytes resq 1
idx   resq 1

; ----------------------------------------------------
section .text
global _start

; ====================================================
; ==========  ENTRY  ==================================
; ====================================================
_start:

; --- Open compiler.case for read ---
    sub rsp, 32
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

; --- Read file ---
    mov rcx,[fd_in]
    lea rdx,[rel buffer]
    mov r8,8192
    lea r9,[rel bytes]
    sub rsp,32
    call ReadFile
    add rsp,32

; --- Create compiler.asm ---
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

; --- Write .data header ---
    mov rcx,[fd_out]
    lea rdx,[rel asm_prologue]
    mov r8,asm_prologue_len
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

; ====================================================
; ==========  PARSE LOOP  =============================
; ====================================================
    lea rsi,[rel buffer]
.next_token:
    lodsb
    cmp al,0
    je .done_parse
    cmp al,'P'
    je .found_print
    cmp al,'F'
    je .found_fn
    jmp .next_token

; ----------------------------------------------------
; Handle Fn main () ( ... ) [end]
; ----------------------------------------------------
.found_fn:
    ; Expect "Fn main"
    mov rcx,[fd_out]
    lea rdx,[rel asm_epilogue]
    mov r8,asm_epilogue_len
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32
    jmp .next_token

; ----------------------------------------------------
; Handle Print "..." [end]
; ----------------------------------------------------
.found_print:
    ; scan until first quote
.scan_quote:
    lodsb
    cmp al,'"'
    jne .scan_quote
    mov rbx,rsi
    mov rcx,0
.copy_text:
    lodsb
    cmp al,'"'
    je .finish_copy
    mov [msg_text+rcx],al
    inc rcx
    jmp .copy_text
.finish_copy:
    mov byte [msg_text+rcx],0

    ; Write msgX db "text",10
    mov rax,[print_count]
    inc rax
    mov [print_count],rax

    ; Build label msgX
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

    ; newline
    mov rcx,[fd_out]
    lea rdx,[rel newline]
    mov r8,2
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

    jmp .next_token

; ----------------------------------------------------
.done_parse:
    ; --- Append exit syscall ---
    mov rcx,[fd_out]
    lea rdx,[rel asm_exitcode]
    mov r8,asm_exit_len
    lea r9,[rel bytes]
    sub rsp,32
    call WriteFile
    add rsp,32

    ; --- Close handles ---
    mov rcx,[fd_in]
    sub rsp,32
    call CloseHandle
    add rsp,32

    mov rcx,[fd_out]
    sub rsp,32
    call CloseHandle
    add rsp,32

; --- Success message ---
    sub rsp,32
    mov rcx,STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx,rax
    lea rdx,[rel ok_msg]
    mov r8,ok_len
    lea r9,[rel bytes]
    call WriteFile
    add rsp,32

; --- Exit ---
    mov ecx,0
    sub rsp,32
    call ExitProcess

; ====================================================
; Constants
; ====================================================
asm_prologue_len equ $-asm_prologue
asm_epilogue_len equ $-asm_epilogue
asm_exit_len     equ $-asm_exitcode
ok_len           equ $-ok_msg
