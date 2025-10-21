# ⚡ C.A.S.E. Quick Reference - Automated Compilation

## 🚀 One-Command Workflow

```bash
transpiler.exe program.case
```

**This does everything:**
1. ✅ Transpiles C.A.S.E. → C++
2. ✅ Compiles C++ → Native binary
3. ✅ Runs the program
4. ✅ Shows output

---

## 📝 Basic Commands

### Windows
```cmd
REM Compile and run
transpiler.exe hello.case

REM Using script
run.bat hello.case

REM Compile only
run.bat program.case --no-run
```

### Linux/macOS
```bash
# Compile and run
./transpiler hello.case

# Using script
./run.sh hello.case

# Compile only
./run.sh program.case --no-run
```

---

## ⚙️ Options

| Option | Effect | Example |
|--------|--------|---------|
| `--no-run` | Don't execute after compile | `run.bat test.case --no-run` |
| `--compiler <name>` | Use specific compiler | `run.bat app.case --compiler g++` |
| `--std <version>` | C++ standard | `run.bat app.case --std c++20` |
| `--opt <level>` | Optimization | `run.bat app.case --opt O3` |
| `--output <name>` | Output filename | `run.bat app.case --output myapp.exe` |
| `--quiet` | Suppress verbose output | `run.bat app.case --quiet` |

---

## 🎯 Common Scenarios

### Quick Test
```bash
echo Print "Hello!" [end] > test.case
transpiler.exe test.case
```

### Development
```bash
# Edit, compile, test loop
notepad program.case
run.bat program.case
# Fix errors, repeat
```

### Production Build
```bash
run.bat release.case --opt O3 --output app.exe --no-run
```

### Debug Build
```bash
run.bat debug.case --opt O0 --no-run
gdb program.exe
```

### Cross-Compiler
```bash
# Use GCC instead of Clang
run.bat app.case --compiler g++

# Use MSVC
run.bat app.case --compiler cl
```

---

## 🔍 Understanding Output

### Success Flow
```
✅ Generated compiler.cpp
✅ Compiled to program.exe
=== Running program.exe ===
[Your program output]
✅ Program executed successfully
```

### Error in C.A.S.E. Code
```
[Error] program.case:5:10
  Expected '[end]' to close statement
[Suggestion] All statements must end with [end]
```

### Error in C++ Compilation
```
❌ Compilation failed
Check compiler.cpp for syntax errors
```

### Program Runtime Error
```
⚠️  Program exited with code: 1
```

---

## 🛠️ Compiler Options

### Optimization Levels
- `O0` - No optimization (fast compile, debugging)
- `O1` - Basic optimization
- `O2` - Standard optimization (default)
- `O3` - Aggressive optimization (best performance)
- `Os` - Size optimization

### C++ Standards
- `c++14` - C++14 features
- `c++17` - C++17 features (default)
- `c++20` - C++20 features
- `c++23` - C++23 features (if supported)

### Compilers
- `clang++` - LLVM Clang (default)
- `g++` - GNU GCC
- `cl` - Microsoft Visual C++

---

## 💡 Tips

### Fast Iteration
```bash
# Quiet mode for less output
run.bat program.case --quiet
```

### Maximum Performance
```bash
# Aggressive optimization
run.bat program.case --opt O3 --std c++20
```

### Debugging
```bash
# No optimization, keep symbols
clang++ -std=c++17 -O0 -g compiler.cpp -o program.exe
gdb program.exe
```

### Custom Output Location
```bash
# Specify output directory
run.bat app.case --output bin/myapp.exe
```

---

## 🐛 Quick Fixes

### "Command not found"
```bash
# Add to PATH or use full path
/full/path/to/transpiler.exe program.case
```

### "Permission denied" (Linux)
```bash
chmod +x run.sh
chmod +x transpiler
```

### "Compiler not found"
```bash
# Install compiler
sudo apt install clang  # Linux
brew install llvm       # macOS
# Or use different compiler
run.bat app.case --compiler g++
```

### Slow Compilation
```bash
# Use faster optimization
run.bat app.case --opt O0
```

---

## 📦 Integration Examples

### Makefile
```makefile
all:
	run.bat program.case --no-run
```

### Batch Processing
```cmd
for %%f in (*.case) do run.bat %%f --quiet
```

### Git Hook (pre-commit)
```bash
#!/bin/bash
./run.sh test.case --no-run
```

---

## 🎓 Learning Path

1. **Start Simple**
   ```bash
   echo Print "Hello" [end] > hello.case
   transpiler.exe hello.case
   ```

2. **Add Complexity**
   ```bash
   # Try examples/
   transpiler.exe examples/calculator.case
   ```

3. **Optimize**
   ```bash
   run.bat myapp.case --opt O3
   ```

4. **Deploy**
   ```bash
   run.bat release.case --opt O3 --output dist/app.exe --no-run
   ```

---

## 📞 Help

```bash
# Windows
run.bat --help

# Linux/macOS
./run.sh --help
```

---

**🔷 Violet Aura Creations**  
**C.A.S.E. Programming Language**  
**"One command, full power"** ⚡
