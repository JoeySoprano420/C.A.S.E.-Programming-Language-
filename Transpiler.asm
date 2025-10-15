; ======================================================
; C.A.S.E. (Compiler Assembler Strategic Execution)
; Phase 1 Bootstrap Transpiler - Windows PE Version
;
; Input : compiler.case   (contains Print "..." [end])
; Output: compiler.asm    (x86-64 NASM source)
;
; Assemble: nasm -f win64 transpiler.asm -o transpiler.obj
; Link:     clang transpiler.obj -o transpiler.exe -Wl,/SUBSYSTEM:CONSOLE,/ENTRY:_start
; Run:      compiler.case → compiler.asm
; ======================================================

; ------------------------------------------------------
; Import WinAPI functions
; ------------------------------------------------------
extern  GetStdHandle
extern  CreateFileA
extern  ReadFile
extern  WriteFile
extern  CloseHandle
extern  ExitProcess

; ------------------------------------------------------
; Constants
; ------------------------------------------------------
STD_OUTPUT_HANDLE equ -11
GENERIC_READ      equ 80000000h
GENERIC_WRITE     equ 40000000h
CREATE_ALWAYS     equ 2
OPEN_EXISTING     equ 3
FILE_SHARE_READ   equ 1
FILE_ATTRIBUTE_NORMAL equ 80h

; ------------------------------------------------------
section .data
infile      db "compiler.case",0
outfile     db "compiler.asm",0

buffer      times 8192 db 0
msg_text    times 512 db 0

; Assembly template components
asm_header  db "section .data",13,10,"msg db ",0
asm_textsec db 13,10,"section .text",13,10,"global _start",13,10,"_start:",13,10,0
asm_sys     db "  mov rax,1",13,10,"  mov rdi,1",13,10,"  lea rsi,[rel msg]",13,10,"  mov rdx,msglen",13,10,"  syscall",13,10,0
asm_exit    db "  mov rax,60",13,10,"  xor rdi,rdi",13,10,"  syscall",13,10,0
asm_footer  db 13,10,"msglen equ $-msg",13,10,0

msg_prefix  db '"',0
msg_suffix  db '"',13,10,0

; Text constants
quote       db '"'
crlf        db 13,10
ok_msg      db "Generated compiler.asm successfully.",13,10,0

; ------------------------------------------------------
section .bss
fd_in   resq 1
fd_out  resq 1
rbytes  resq 1
wbytes  resq 1
idx     resq 1

; ------------------------------------------------------
section .text
global _start

; ------------------------------------------------------
_start:

; --- Open compiler.case for reading ---
    sub rsp, 32                         ; shadow space
    lea rcx, [rel infile]
    mov rdx, GENERIC_READ
    mov r8, FILE_SHARE_READ
    mov r9, 0                           ; lpSecurityAttr
    push FILE_ATTRIBUTE_NORMAL
    push OPEN_EXISTING
    push 0                              ; template
    call CreateFileA
    add rsp, 32 + 8*3
    mov [fd_in], rax

; --- Read file contents into buffer ---
    mov rcx, [fd_in]
    lea rdx, [rel buffer]
    mov r8, 8192
    lea r9, [rel rbytes]
    sub rsp, 32
    call ReadFile
    add rsp, 32

; --- Find quoted string after Print ---
    lea rsi, [rel buffer]
find_quote:
    lodsb
    cmp al, '"'
    jne find_quote
    mov rbx, rsi
    mov rcx, 0
copy_loop:
    lodsb
    cmp al, '"'
    je done_copy
    mov [msg_text+rcx], al
    inc rcx
    jmp copy_loop
done_copy:
    mov byte [msg_text+rcx], 0

; --- Open compiler.asm for writing ---
    sub rsp, 32
    lea rcx, [rel outfile]
    mov rdx, GENERIC_WRITE
    mov r8, 0
    mov r9, 0
    push FILE_ATTRIBUTE_NORMAL
    push CREATE_ALWAYS
    push 0
    call CreateFileA
    add rsp, 32 + 8*3
    mov [fd_out], rax

; --- Write header ---
    mov rcx, [fd_out]
    lea rdx, [rel asm_header]
    mov r8, strlen_header
    lea r9, [rel wbytes]
    sub rsp, 32
    call WriteFile
    add rsp, 32

; --- Write "msg db "<text>" etc. ---
    mov rcx, [fd_out]
    lea rdx, [rel msg_prefix]
    mov r8, 1
    lea r9, [rel wbytes]
    sub rsp, 32
    call WriteFile
    add rsp, 32

    mov rcx, [fd_out]
    lea rdx, [rel msg_text]
    mov r8, rcx                        ; use rcx as len
    mov r8, rcx
    lea r9, [rel wbytes]
    sub rsp, 32
    call WriteFile
    add rsp, 32

    mov rcx, [fd_out]
    lea rdx, [rel msg_suffix]
    mov r8, 2
    lea r9, [rel wbytes]
    sub rsp, 32
    call WriteFile
    add rsp, 32

; --- Footer + text section ---
    mov rcx, [fd_out]
    lea rdx, [rel asm_footer]
    mov r8, strlen_footer
    lea r9, [rel wbytes]
    sub rsp, 32
    call WriteFile
    add rsp, 32

    mov rcx, [fd_out]
    lea rdx, [rel asm_textsec]
    mov r8, strlen_textsec
    lea r9, [rel wbytes]
    sub rsp, 32
    call WriteFile
    add rsp, 32

    mov rcx, [fd_out]
    lea rdx, [rel asm_sys]
    mov r8, strlen_sys
    lea r9, [rel wbytes]
    sub rsp, 32
    call WriteFile
    add rsp, 32

    mov rcx, [fd_out]
    lea rdx, [rel asm_exit]
    mov r8, strlen_exit
    lea r9, [rel wbytes]
    sub rsp, 32
    call WriteFile
    add rsp, 32

; --- Close handles ---
    mov rcx, [fd_in]
    sub rsp, 32
    call CloseHandle
    add rsp, 32

    mov rcx, [fd_out]
    sub rsp, 32
    call CloseHandle
    add rsp, 32

; --- Print completion message ---
    sub rsp, 32
    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax
    lea rdx, [rel ok_msg]
    mov r8, ok_len
    lea r9, [rel wbytes]
    call WriteFile
    add rsp, 32

; --- Exit cleanly ---
    mov ecx, 0
    sub rsp, 32
    call ExitProcess

; ------------------------------------------------------
; Data sizes
; ------------------------------------------------------
strlen_header  equ $-asm_header
strlen_footer  equ $-asm_footer
strlen_textsec equ $-asm_textsec
strlen_sys     equ $-asm_sys
strlen_exit    equ $-asm_exit
ok_len         equ $-ok_msg
