# CIAM AOT Quick Reference
## Machine Code Emitter API

---

## 🎯 CIAM INSTRUCTION MACROS

### **Data Movement**

```cpp
// MOV: Move immediate to register
CIAM::X64Builder::MOV_REG_IMM(Reg dst, uint64_t imm)
// Example: mov rax, 42
auto inst = MOV_REG_IMM(Reg::RAX, 42);

// LEA: Load Effective Address
CIAM::X64Builder::LEA_REG_MEM(Reg dst, Reg base, int32_t offset)
// Example: lea rsi, [rip + 0x100]
auto inst = LEA_REG_MEM(Reg::RSI, Reg::RIP, 0x100);
```

### **Arithmetic**

```cpp
// ADD: Add registers
CIAM::X64Builder::ADD_REG_REG(Reg dst, Reg src)
// Example: add rax, rcx
auto inst = ADD_REG_REG(Reg::RAX, Reg::RCX);

// SUB: Subtract registers
CIAM::X64Builder::SUB_REG_REG(Reg dst, Reg src)
// Example: sub rdx, rbx
auto inst = SUB_REG_REG(Reg::RDX, Reg::RBX);

// IMUL: Signed multiply
CIAM::X64Builder::IMUL_REG_REG(Reg dst, Reg src)
// Example: imul rax, rcx
auto inst = IMUL_REG_REG(Reg::RAX, Reg::RCX);
```

### **Stack Operations**

```cpp
// PUSH: Push register to stack
CIAM::X64Builder::PUSH_REG(Reg reg)
// Example: push rbp
auto inst = PUSH_REG(Reg::RBP);

// POP: Pop from stack to register
CIAM::X64Builder::POP_REG(Reg reg)
// Example: pop rbp
auto inst = POP_REG(Reg::RBP);
```

### **Control Flow**

```cpp
// CALL: Call function (relative offset)
CIAM::X64Builder::CALL_REL32(int32_t offset)
// Example: call +0x50
auto inst = CALL_REL32(0x50);

// RET: Return from function
CIAM::X64Builder::RET()
// Example: ret
auto inst = RET();

// JMP: Unconditional jump
CIAM::X64Builder::JMP_REL32(int32_t offset)
// Example: jmp +0x100
auto inst = JMP_REL32(0x100);

// JE: Jump if equal (ZF=1)
CIAM::X64Builder::JE_REL32(int32_t offset)
// Example: je +0x20
auto inst = JE_REL32(0x20);
```

### **Comparison**

```cpp
// CMP: Compare registers
CIAM::X64Builder::CMP_REG_REG(Reg left, Reg right)
// Example: cmp rax, rcx
auto inst = CMP_REG_REG(Reg::RAX, Reg::RCX);
```

### **Logical**

```cpp
// XOR: Exclusive OR (often used to zero register)
CIAM::X64Builder::XOR_REG_REG(Reg reg)
// Example: xor rax, rax
auto inst = XOR_REG_REG(Reg::RAX);
```

### **System Calls**

```cpp
// SYSCALL: Linux system call
CIAM::X64Builder::SYSCALL()
// Example: syscall
auto inst = SYSCALL();

// INT: Interrupt (Windows)
CIAM::X64Builder::INT_IMM8(uint8_t vector)
// Example: int 0x21
auto inst = INT_IMM8(0x21);
```

---

## 🗄️ REGISTER ALLOCATOR

### **Usage:**

```cpp
RegisterAllocator regAlloc;

// Allocate register for variable
CIAM::Reg reg = regAlloc.allocate("myVar");

// Get register for existing variable
CIAM::Reg reg = regAlloc.getReg("myVar");

// Free register
regAlloc.free("myVar");
regAlloc.free(Reg::RAX);
```

### **Available Registers:**

```cpp
enum class Reg : uint8_t {
    // General purpose (64-bit)
 RAX = 0,   // Accumulator, return value
RCX = 1,   // Counter, 4th argument (Windows)
    RDX = 2,   // Data, 3rd argument (Linux)
  RBX = 3,   // Base
    RSP = 4,   // Stack pointer (reserved)
    RBP = 5, // Base pointer (reserved)
    RSI = 6,   // Source index, 2nd argument
    RDI = 7,   // Destination index, 1st argument
    
    // Extended registers
    R8  = 8,   // 5th argument (Windows)
    R9  = 9,   // 6th argument (Windows)
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
 R15 = 15,
    
    NONE = 0xFF  // No register
};
```

---

## 📦 CODE SECTION

### **Basic Operations:**

```cpp
CIAM::CodeSection section;

// Get current code offset
uint32_t offset = section.currentOffset();

// Emit label
section.emitLabel("my_function");

// Emit instruction bytes
auto inst = CIAM::X64Builder::MOV_REG_IMM(Reg::RAX, 42);
section.emitBytes(inst.bytes);

// Add relocation (for jumps/calls to labels)
section.addRelocation("target_label");
```

### **Labels and Relocations:**

```cpp
// Create label
section.emitLabel("loop_start");

// ... emit code ...

// Jump back to label (relocation will be resolved)
section.addRelocation("loop_start");
section.emitBytes(CIAM::X64Builder::JMP_REL32(0).bytes);
```

---

## 🏗️ MACHINE CODE EMITTER

### **Main Interface:**

```cpp
MachineCodeEmitter emitter;

// Emit entire AST to machine code
std::vector<uint8_t> code = emitter.emit(astRoot);
```

### **Node-Specific Emission:**

```cpp
// Emit statement
void emitNode(NodePtr node);

// Emit expression (returns register with result)
CIAM::Reg emitExpr(NodePtr expr);

// Specialized emitters
void emitPrint(NodePtr expr);
void emitFunction(const std::string& name, NodePtr body);
void emitReturn(NodePtr value);
void emitIfStmt(NodePtr condition, NodePtr thenBlock, NodePtr elseBlock);
void emitWhileStmt(NodePtr condition, NodePtr block);
void emitVarDecl(const std::string& name, NodePtr initializer);
```

---

## 📝 BINARY EMITTER

### **Platform Detection:**

```cpp
PlatformInfo info = detectPlatform();

// Check platform
if (info.platform == Platform::Windows) { /* ... */ }
if (info.format == ExecutableFormat::PE) { /* ... */ }
```

### **Binary Writing:**

```cpp
// Automatic platform selection
BinaryWriter::writeBinary("output.exe", codeBytes, dataBytes);

// Manual platform-specific emitters
PEEmitter peEmitter;
peEmitter.emitExecutable("output.exe", code, data);

ELFEmitter elfEmitter;
elfEmitter.emitExecutable("output", code, data);

MachOEmitter machoEmitter;
machoEmitter.emitExecutable("output", code, data);
```

---

## 🔧 SYSTEM CALL CONVENTIONS

### **Linux (x86-64)**

```cpp
// sys_write (syscall #1)
// RAX = 1
// RDI = file descriptor (1 = stdout)
// RSI = buffer pointer
// RDX = length

section.emitBytes(MOV_REG_IMM(Reg::RAX, 1).bytes);
section.emitBytes(MOV_REG_IMM(Reg::RDI, 1).bytes);
section.emitBytes(LEA_REG_MEM(Reg::RSI, Reg::RIP, strOffset).bytes);
section.emitBytes(MOV_REG_IMM(Reg::RDX, length).bytes);
section.emitBytes(SYSCALL().bytes);

// sys_exit (syscall #60)
// RAX = 60
// RDI = exit code

section.emitBytes(MOV_REG_IMM(Reg::RAX, 60).bytes);
section.emitBytes(MOV_REG_IMM(Reg::RDI, 0).bytes);
section.emitBytes(SYSCALL().bytes);
```

### **Windows (x86-64)**

```cpp
// Windows uses different calling convention
// RCX = 1st argument
// RDX = 2nd argument
// R8  = 3rd argument
// R9  = 4th argument

// ExitProcess(exitCode)
section.emitBytes(MOV_REG_IMM(Reg::RCX, exitCode).bytes);
// ... call ExitProcess via import table ...
```

---

## 🎨 COMMON PATTERNS

### **Function Prologue:**

```cpp
section.emitLabel(functionName);
section.emitBytes(PUSH_REG(Reg::RBP).bytes);
section.emitBytes({0x48, 0x89, 0xE5});  // mov rbp, rsp
```

### **Function Epilogue:**

```cpp
section.emitBytes({0x48, 0x89, 0xEC});  // mov rsp, rbp
section.emitBytes(POP_REG(Reg::RBP).bytes);
section.emitBytes(RET().bytes);
```

### **If Statement:**

```cpp
// Evaluate condition
CIAM::Reg condReg = emitExpr(condition);

// Compare with zero
section.emitBytes(XOR_REG_REG(Reg::R11).bytes);
section.emitBytes(CMP_REG_REG(condReg, Reg::R11).bytes);

// Jump to else if false
section.addRelocation("else_label");
section.emitBytes(JE_REL32(0).bytes);

// Then block
emitNode(thenBlock);

// Jump to end
section.addRelocation("end_label");
section.emitBytes(JMP_REL32(0).bytes);

// Else block
section.emitLabel("else_label");
emitNode(elseBlock);

// End
section.emitLabel("end_label");
```

### **While Loop:**

```cpp
section.emitLabel("loop_start");

// Evaluate condition
CIAM::Reg condReg = emitExpr(condition);
section.emitBytes(XOR_REG_REG(Reg::R11).bytes);
section.emitBytes(CMP_REG_REG(condReg, Reg::R11).bytes);

// Exit loop if false
section.addRelocation("loop_end");
section.emitBytes(JE_REL32(0).bytes);

// Loop body
emitNode(loopBody);

// Jump back to start
section.addRelocation("loop_start");
section.emitBytes(JMP_REL32(0).bytes);

section.emitLabel("loop_end");
```

---

## 📊 INSTRUCTION ENCODING REFERENCE

### **REX Prefix (64-bit operations):**

```
REX.W = 0x48 | (R << 2) | (X << 1) | B
  W = 1 for 64-bit operand size
  R = Extension of ModR/M reg field
  X = Extension of SIB index field
  B = Extension of ModR/M r/m, SIB base, or opcode reg field
```

### **ModR/M Byte:**

```
ModR/M = (Mod << 6) | (Reg << 3) | R/M
  Mod = Addressing mode (11 = register direct)
  Reg = Register operand
  R/M = Register or memory operand
```

### **Common Opcodes:**

```cpp
0xB8 + r     // MOV r64, imm64 (with REX.W)
0x01         // ADD r/m64, r64
0x29// SUB r/m64, r64
0x0F 0xAF  // IMUL r64, r/m64
0x50 + r     // PUSH r64
0x58 + r   // POP r64
0xE8     // CALL rel32
0xC3         // RET
0xE9         // JMP rel32
0x0F 0x84    // JE rel32
0x39 // CMP r/m64, r64
0x31         // XOR r/m64, r64
0x8D         // LEA r64, m
0x0F 0x05    // SYSCALL
0xCD       // INT imm8
```

---

## 🚀 OPTIMIZATION TIPS

### **Register Reuse:**

```cpp
// Bad: Allocate new register every time
CIAM::Reg r1 = regAlloc.allocate("temp1");
CIAM::Reg r2 = regAlloc.allocate("temp2");
// ...

// Good: Reuse registers
CIAM::Reg temp = regAlloc.allocate("temp");
// ... use temp ...
regAlloc.free(temp);
// ... reuse temp ...
```

### **Instruction Selection:**

```cpp
// Zero a register: XOR is smaller and faster than MOV
// mov rax, 0      -> 7 bytes
// xor rax, rax  -> 3 bytes
section.emitBytes(XOR_REG_REG(Reg::RAX).bytes);
```

### **Tail Calls:**

```cpp
// Instead of:
section.emitBytes(CALL_REL32(offset).bytes);
section.emitBytes(RET().bytes);

// Use:
section.emitBytes(JMP_REL32(offset).bytes);
```

---

## 📚 ADDITIONAL RESOURCES

- **Intel Manual**: Intel® 64 and IA-32 Architectures Software Developer's Manual (Vol. 2)
- **x86-64 ABI**: System V Application Binary Interface AMD64 Architecture Processor Supplement
- **Online Tools**: [godbolt.org](https://godbolt.org) (Compiler Explorer)
- **Disassembler**: `objdump -d` (Linux), `dumpbin /disasm` (Windows)

---

*Violet Aura Creations — CIAM AOT Technology*
