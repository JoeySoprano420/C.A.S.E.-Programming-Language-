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

