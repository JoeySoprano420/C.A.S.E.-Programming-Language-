

⸻

🧩 Using Clang (LLVM for Windows)

Clang can directly handle NASM-style object files (-f win64) and invoke MSVC’s linker automatically:

nasm -f win64 compiler.asm -o compiler.obj
clang compiler.obj -o compiler.exe -Wl,/SUBSYSTEM:CONSOLE,/ENTRY:_start
compiler.exe

🧠 Breakdown
	•	nasm -f win64 — emits a 64-bit Windows COFF object.
	•	clang compiler.obj — compiles/links with MSVC-compatible backend.
	•	-Wl, prefix — passes flags directly to the linker (/SUBSYSTEM and /ENTRY).
	•	/ENTRY:_start — specifies your custom entry symbol.
	•	/SUBSYSTEM:CONSOLE — ensures it opens a terminal console.
	•	Output: compiler.exe

nasm -f win64 transpiler.asm -o transpiler.obj
clang transpiler.obj -o transpiler.exe -Wl,/SUBSYSTEM:CONSOLE,/ENTRY:_start

⸻

🧱 Using Pure MSVC (Developer Command Prompt)

If you’re inside Visual Studio Developer Command Prompt:

assemble & link:
nasm -f win64 compiler.asm -o compiler.obj
link /SUBSYSTEM:CONSOLE /ENTRY:_start /OUT:compiler.exe compiler.obj
compiler.exe

run:
transpiler.exe

build:
nasm -f win64 compiler.asm -o compiler.obj
clang compiler.obj -o compiler.exe -Wl,/SUBSYSTEM:CONSOLE,/ENTRY:_start
compiler.exe


🧩 Breakdown
	•	link.exe is Microsoft’s native linker.
	•	/ENTRY:_start → entry point matches your NASM label.
	•	/SUBSYSTEM:CONSOLE → console mode app.
	•	/OUT: → sets executable name.
	•	Output: compiler.exe

⸻

✅ Summary

Toolchain	Command Line
Clang (LLVM)	nasm -f win64 compiler.asm -o compiler.objclang compiler.obj -o compiler.exe -Wl,/SUBSYSTEM:CONSOLE,/ENTRY:_start
MSVC (link.exe)	nasm -f win64 compiler.asm -o compiler.objlink /SUBSYSTEM:CONSOLE /ENTRY:_start /OUT:compiler.exe compiler.obj


⸻

