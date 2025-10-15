section .data
msg db "Hello from Windows C.A.S.E."
msglen equ $-msg

section .text
global _start
_start:
  mov rax,1
  mov rdi,1
  lea rsi,[rel msg]
  mov rdx,msglen
  syscall
  mov rax,60
  xor rdi,rdi
  syscall
