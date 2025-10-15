section .data
msg db "Hello from main!"
msg db "Condition true"
msg db "Condition false"
msg db "Inside secondary fn"

section .text
global _start

_start:
  mov rax,1
  mov rdi,1
  lea rsi,[rel msg]
  mov rdx,msglen
  syscall

if_1:
  ; (conditional code placeholder)
  mov rax,1
  mov rdi,1
  lea rsi,[rel msg]
  mov rdx,msglen
  syscall

endif_1:
  mov rax,60
  xor rdi,rdi
  syscall
