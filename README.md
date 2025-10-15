# C.A.S.E.-Programming-Language-

phase1_transpiler.asm → compiler.case → compiler.asm → compiler.exe

compiler.case (written in C.A.S.E.)
  ↓
bootstrap transpiler → compiler.asm
  ↓
assembler/linker → compiler.exe
  ↓
compiler.exe → can compile new .case files

