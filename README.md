# C.A.S.E.-Programming-Language-

phase1_transpiler.asm → compiler.case → compiler.asm → compiler.exe

-----

compiler.case (written in C.A.S.E.)
  ↓
bootstrap transpiler → compiler.asm
  ↓
assembler/linker → compiler.exe
  ↓
compiler.exe → can compile new .case files

-----

🧱 Build Pipeline

# 1. Use your existing bootstrap transpiler (Phase 1–2)
transpiler.exe

# 2. The transpiler converts compiler.case → compiler.asm
nasm -f win64 compiler.asm -o compiler.obj
clang compiler.obj -o compiler.exe -Wl,/SUBSYSTEM:CONSOLE,/ENTRY:_start

# 3. Run the self-compiled compiler
compiler.exe

-----

🧬 What You Now Have

| Layer            | Language        | Description                                |
| ---------------- | --------------- | ------------------------------------------ |
| Phase 1          | x86-64 ASM      | Minimal transpiler                         |
| Phase 2          | x86-64 ASM      | Conditional parser                         |
| Phase 3          | C.A.S.E.        | Self-hosting compiler source               |
| Phase 3.5 (next) | C.A.S.E. + CIAM | Adds macros, base-12 math, plugin overlays |

-----

🧩 Implementation Outline for case_backend.asm

| Stage | Routine              | Description                                                             |
| ----- | -------------------- | ----------------------------------------------------------------------- |
| 1     | **read_source**      | Opens and reads `input.case` into memory.                               |
| 2     | **parse_case**       | Detects `Fn`, `Print`, `[end]`. Records entries `{op, text, fn_index}`. |
| 3     | **select_mode**      | If total IR > threshold → set `mode = CALLABLE` else `INLINE`.          |
| 4     | **emit_header**      | Writes `.data`, `.text`, and `_start:` to `output.asm`.                 |
| 5     | **emit_inline_fn**   | Writes prints directly inside `_start:` (Mode A).                       |
| 6     | **emit_callable_fn** | Writes labels and a `call main / ret` structure (Mode B).               |
| 7     | **finalize_output**  | Appends inline strings and WinAPI print stubs.                          |

All emission still uses WinAPI:
GetStdHandle, WriteFile, ExitProcess.

-----

🧱 Behavior Example
input.case

Fn main () (
  Print "Hello from inline" [end]
) [end]

Fn extra () (
  Print "Secondary function" [end]
) [end]


output.asm (Mode A)

section .text
global _start
_start:
  ; Inline Fn main
  sub rsp,32
  mov rcx,-11
  call GetStdHandle
  mov rcx,rax
  lea rdx,[rel _s1]
  mov r8,_s1e-_s1
  lea r9,[rel bytes]
  call WriteFile
  add rsp,32

  ; Inline Fn extra
  sub rsp,32
  mov rcx,-11
  call GetStdHandle
  mov rcx,rax
  lea rdx,[rel _s2]
  mov r8,_s2e-_s2
  lea r9,[rel bytes]
  call WriteFile
  add rsp,32

  sub rsp,32
  mov ecx,0
  call ExitProcess

_s1 db "Hello from inline",13,10,0
_s1e:
_s2 db "Secondary function",13,10,0
_s2e:
bytes dq 0


output.asm (Mode B)

section .text
global _start
_start:
  call main
  sub rsp,32
  mov ecx,0
  call ExitProcess
  ret

main:
  ; from Fn main
  sub rsp,32
  mov rcx,-11
  call GetStdHandle
  mov rcx,rax
  lea rdx,[rel _s1]
  mov r8,_s1e-_s1
  lea r9,[rel bytes]
  call WriteFile
  add rsp,32
  ret

extra:
  ; from Fn extra
  sub rsp,32
  mov rcx,-11
  call GetStdHandle
  mov rcx,rax
  lea rdx,[rel _s2]
  mov r8,_s2e-_s2
  lea r9,[rel bytes]
  call WriteFile
  add rsp,32
  ret

_s1 db "Hello from inline",13,10,0
_s1e:
_s2 db "Secondary function",13,10,0
_s2e:
bytes dq 0

-----

⚙️ Parameters

INLINE_THRESHOLD equ 8192      ; bytes of generated IR
MODE_INLINE      equ 0
MODE_CALLABLE    equ 1

Backend logic:

cmp [ir_size], INLINE_THRESHOLD
jae switch_to_callable

