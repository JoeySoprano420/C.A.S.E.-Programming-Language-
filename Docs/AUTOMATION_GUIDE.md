# 🚀 C.A.S.E. Automated Native Compilation & Execution Guide

## Overview

The C.A.S.E. transpiler now features **fully automated native compilation and execution**, allowing you to compile and run C.A.S.E. programs with a single command.

---

## ✨ Features

### Built-in Automation
- ✅ Automatic C++ compilation after transpilation
- ✅ Automatic program execution after successful compilation
- ✅ Real-time output streaming
- ✅ Comprehensive error reporting
- ✅ Exit code handling
- ✅ Color-coded console output

### Command-Line Scripts
- ✅ Windows batch script (`run.bat`)
- ✅ Linux/macOS shell script (`run.sh`)
- ✅ Configurable compiler options
- ✅ Flexible execution control

---

## 🔧 Usage

### Method 1: Direct Transpiler (Simplest)

```bash
# Compile and run automatically
transpiler.exe hello.case
```

**This automatically:**
1. Transpiles `hello.case` → `compiler.cpp`
2. Compiles `compiler.cpp` → `program.exe`
3. Executes `program.exe`
4. Shows output in real-time

### Method 2: Windows Batch Script

```cmd
REM Basic usage
run.bat hello.case

REM Don't run after compilation
run.bat test.case --no-run

REM Use different compiler
run.bat program.case --compiler g++

REM Custom C++ standard
run.bat advanced.case --std c++20

REM Optimization level
run.bat optimized.case --opt O3

REM Custom output name
run.bat myprogram.case --output myapp.exe

REM Quiet mode
run.bat silent.case --quiet

REM Combine options
run.bat full.case --compiler g++ --std c++20 --opt O3 --output myapp.exe
```

### Method 3: Linux/macOS Shell Script

```bash
# Make script executable (first time only)
chmod +x run.sh

# Basic usage
./run.sh hello.case

# Don't run after compilation
./run.sh test.case --no-run

# Use different compiler
./run.sh program.case --compiler g++

# Custom C++ standard
./run.sh advanced.case --std c++20

# Optimization level
./run.sh optimized.case --opt O3

# Custom output name
./run.sh myprogram.case --output myapp

# Quiet mode
./run.sh silent.case --quiet

# Show help
./run.sh --help

# Combine options
./run.sh full.case --compiler g++ --std c++20 --opt O3 --output myapp
```

---

## 📋 Command-Line Options

### Direct Transpiler Options

The transpiler executable doesn't currently support command-line flags, but the scripts provide these options:

| Option | Description | Default |
|--------|-------------|---------|
| `<input.case>` | Input C.A.S.E. file | *Required* |
| `--no-run` | Compile but don't execute | Auto-run enabled |
| `--compiler <name>` | C++ compiler to use | `clang++` |
| `--std <version>` | C++ standard version | `c++17` |
| `--opt <level>` | Optimization level | `O2` |
| `--output <name>` | Output executable name | `program.exe` / `program` |
| `--quiet` | Suppress verbose output | Verbose enabled |

---

## 🎯 Examples

### Example 1: Hello World

**hello.case:**
```case
Print "Hello, World!" [end]
```

**Compile and run:**
```bash
transpiler.exe hello.case
```

**Output:**
```
=== Token Stream ===
...

=== AST ===
...

✅ Generated compiler.cpp

=== Compiling C++ ===
Command: clang++ -std=c++17 -O2 compiler.cpp -o program.exe 2>&1
✅ Compiled to program.exe

=== Running program.exe ===

Hello, World!

✅ Program executed successfully
```

### Example 2: Calculator

**calculator.case:**
```case
let x = 10 [end]
let y = 5 [end]
let sum = x + y [end]
Print "Sum: " [end]
Print sum [end]
```

**Compile and run:**
```bash
run.bat calculator.case
```

### Example 3: Fibonacci

**fibonacci.case:**
```case
Fn fibonacci "n" (
    if n <= 1 {
        ret n
    }
    let a = call fibonacci (n - 1) [end]
    let b = call fibonacci (n - 2) [end]
    ret a + b
) [end]

let result = call fibonacci 10 [end]
Print "Fibonacci(10) = " [end]
Print result [end]
```

**Compile with optimization:**
```bash
run.bat fibonacci.case --opt O3
```

### Example 4: Development Workflow

**Compile without running (for debugging):**
```bash
run.bat program.case --no-run
```

Then manually run with debugger:
```bash
gdb program.exe
```

### Example 5: Cross-Platform Build

**Windows (MSVC):**
```cmd
run.bat program.case --compiler cl --std c++17
```

**Linux (GCC):**
```bash
./run.sh program.case --compiler g++ --std c++20
```

**macOS (Clang):**
```bash
./run.sh program.case --compiler clang++ --std c++17
```

---

## 🔍 Output Explained

### Stage 1: Transpilation

```
=== Token Stream ===
    1:  1 | Keyword      -> "Print"
    1:  7 | String       -> "Hello"
    1: 14 | Keyword      -> "end"
```

Shows the tokenized C.A.S.E. code.

### Stage 2: Parsing

```
=== AST ===
Program
  Print
    StringLiteral: "Hello"
```

Shows the Abstract Syntax Tree structure.

### Stage 3: Code Generation

```
✅ Generated compiler.cpp
```

C++ code has been generated.

### Stage 4: C++ Compilation

```
=== Compiling C++ ===
Command: clang++ -std=c++17 -O2 compiler.cpp -o program.exe 2>&1
✅ Compiled to program.exe
```

C++ compilation status and command used.

### Stage 5: Execution

```
=== Running program.exe ===

Hello, World!

✅ Program executed successfully
```

Program output and exit status.

---

## 🚨 Error Handling

### Transpilation Errors

**Example: Missing `[end]`**

**Input (bad.case):**
```case
Print "Hello"
```

**Output:**
```
[Error] bad.case:1:14
  Expected '[end]' to close statement

    1 | Print "Hello"
  ^

[Suggestion] All statements must end with [end]

=== Compilation Summary ===
1 error(s)
```

### C++ Compilation Errors

**Example: Generated invalid C++**

**Output:**
```
=== Compiling C++ ===
Command: clang++ -std=c++17 -O2 compiler.cpp -o program.exe 2>&1
❌ Compilation failed
Check compiler.cpp for syntax errors

compiler.cpp:10:5: error: expected ';' after expression
```

**Action:** Check `compiler.cpp` to see the generated C++ code.

### Runtime Errors

**Example: Program crashes or exits with error**

**Output:**
```
=== Running program.exe ===

Segmentation fault

⚠️  Program exited with code: 139
```

---

## ⚙️ Configuration

### Compiler Selection

**Use GCC:**
```bash
run.bat program.case --compiler g++
```

**Use MSVC (Windows):**
```cmd
run.bat program.case --compiler cl
```

**Use Clang:**
```bash
./run.sh program.case --compiler clang++
```

### C++ Standard

**C++14:**
```bash
run.bat program.case --std c++14
```

**C++17 (default):**
```bash
run.bat program.case --std c++17
```

**C++20:**
```bash
run.bat program.case --std c++20
```

### Optimization Levels

| Level | Description | Use Case |
|-------|-------------|----------|
| `O0` | No optimization | Fast compile, debugging |
| `O1` | Basic optimization | Balanced |
| `O2` | Moderate optimization (default) | Production |
| `O3` | Aggressive optimization | Maximum performance |
| `Os` | Size optimization | Embedded systems |

**Example:**
```bash
run.bat program.case --opt O3
```

---

## 🎬 Complete Workflow Examples

### Example 1: Quick Test

```bash
# Edit hello.case
echo Print "Hello, C.A.S.E.!" [end] > hello.case

# Run immediately
transpiler.exe hello.case
```

### Example 2: Development Cycle

```bash
# 1. Write code
notepad program.case

# 2. Compile and test
run.bat program.case

# 3. If errors, fix and retry
notepad program.case
run.bat program.case

# 4. Deploy (compile without running)
run.bat program.case --no-run --opt O3 --output myapp.exe
```

### Example 3: Batch Processing

```cmd
REM Compile multiple programs
for %%f in (*.case) do (
    echo Compiling %%f...
    run.bat %%f --no-run
)
```

### Example 4: CI/CD Pipeline

```bash
#!/bin/bash
# ci-build.sh

for file in tests/*.case; do
    echo "Testing $file..."
    ./run.sh "$file" --quiet
if [ $? -ne 0 ]; then
        echo "FAILED: $file"
        exit 1
    fi
done
echo "All tests passed!"
```

---

## 📊 Performance Tips

### 1. Use Optimization Flags

```bash
run.bat program.case --opt O3
```

### 2. Use Appropriate C++ Standard

```bash
# Modern features (faster STL)
run.bat program.case --std c++20
```

### 3. Compiler-Specific Optimizations

**Clang with LTO (Link-Time Optimization):**
```bash
clang++ -std=c++17 -O3 -flto compiler.cpp -o program.exe
```

**GCC with Profile-Guided Optimization:**
```bash
# First run with profiling
g++ -std=c++17 -O2 -fprofile-generate compiler.cpp -o program.exe
./program.exe

# Compile with profile data
g++ -std=c++17 -O2 -fprofile-use compiler.cpp -o program.exe
```

### 4. Parallel Compilation

For large projects, use parallel compilation:
```bash
clang++ -std=c++17 -O2 -j8 compiler.cpp -o program.exe
```

---

## 🐛 Troubleshooting

### Problem: "Command not found: transpiler.exe"

**Solution:**
```bash
# Add transpiler directory to PATH
export PATH=$PATH:/path/to/transpiler
```

Or use full path:
```bash
/full/path/to/transpiler.exe hello.case
```

### Problem: "Compiler not found: clang++"

**Solution (Windows):**
1. Install LLVM/Clang
2. Add to PATH

Or use alternative:
```bash
run.bat program.case --compiler g++
```

**Solution (Linux):**
```bash
sudo apt install clang  # Ubuntu/Debian
sudo yum install clang  # CentOS/RHEL
```

### Problem: "Permission denied: ./run.sh"

**Solution:**
```bash
chmod +x run.sh
```

### Problem: Generated program crashes

**Solution:**
1. Compile with debug info:
```bash
clang++ -std=c++17 -g compiler.cpp -o program.exe
```

2. Run with debugger:
```bash
gdb program.exe
```

### Problem: Slow compilation

**Solution:**
- Use faster optimizer: `--opt O0`
- Use precompiled headers
- Reduce code complexity

---

## 📚 Advanced Usage

### Custom Build Scripts

Create your own build script:

**build.bat:**
```cmd
@echo off
transpiler.exe %1
if errorlevel 1 exit /b 1

clang++ -std=c++17 -O3 -Wall -Wextra compiler.cpp -o %2
if errorlevel 1 exit /b 1

echo Build successful: %2
```

Usage:
```bash
build.bat input.case output.exe
```

### Integration with Make

**Makefile:**
```makefile
TRANSPILER = transpiler.exe
CXX = clang++
CXXFLAGS = -std=c++17 -O2

%.exe: %.case
	$(TRANSPILER) $<
	$(CXX) $(CXXFLAGS) compiler.cpp -o $@

all: program.exe

clean:
	rm -f compiler.cpp program.exe

.PHONY: all clean
```

Usage:
```bash
make program.exe
```

### Integration with CMake

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.10)
project(CASEProgram)

set(CMAKE_CXX_STANDARD 17)

add_custom_command(
    OUTPUT compiler.cpp
    COMMAND transpiler.exe ${CMAKE_SOURCE_DIR}/program.case
    DEPENDS program.case
)

add_executable(program compiler.cpp)
```

Usage:
```bash
cmake -B build
cmake --build build
```

---

## 🎓 Best Practices

### 1. Keep Source Files Organized

```
project/
├── src/
│   ├── main.case
│   ├── utils.case
│   └── lib.case
├── build/
│   └── (generated files)
└── bin/
    └── (executables)
```

### 2. Use Version Control

```bash
# .gitignore
compiler.cpp
program.exe
*.o
*.obj
build/
```

### 3. Automated Testing

```bash
# test.sh
for test in tests/*.case; do
    ./run.sh "$test" --quiet
done
```

### 4. Continuous Integration

**GitHub Actions:**
```yaml
name: Build

on: [push]

jobs:
  build:
  runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Compile
        run: ./run.sh program.case --no-run
```

---

## 📖 Summary

The automated native compilation and execution system provides:

✅ **One-command workflow**: From `.case` to running program  
✅ **Flexible configuration**: Multiple compilers, standards, optimizations  
✅ **Cross-platform support**: Windows, Linux, macOS  
✅ **Error transparency**: Clear error messages at every stage  
✅ **Production ready**: Optimized builds with single command  

**Get started now:**
```bash
transpiler.exe your_program.case
```

---

**🔷 Violet Aura Creations**  
**C.A.S.E. Programming Language**  
**"From code to execution in seconds"** ⚡
