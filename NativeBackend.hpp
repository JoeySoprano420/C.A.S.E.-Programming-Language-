//=============================================================================
//  🌌 Violet Aura Creations — CASE P Native Code Generation Backend
//  Project: CASE Lang Native Compiler Backend
//  Target : Windows x64 | PE Executable | Native Machine Code
//  File   : NativeBackend.hpp
//=============================================================================

#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>
#include <fstream>

namespace CASE {
namespace Native {

// -----------------------------------------------------------------------------
// X86_64 Register Definitions
// -----------------------------------------------------------------------------

enum class X64Register : uint8_t {
    // General Purpose Registers (64-bit)
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    
    // 32-bit equivalents
    EAX = 16, ECX = 17, EDX = 18, EBX = 19,
    
    // XMM registers for floating point
    XMM0 = 32, XMM1 = 33, XMM2 = 34, XMM3 = 35,
    XMM4 = 36, XMM5 = 37, XMM6 = 38, XMM7 = 39,
    
    NONE = 255
};

// -----------------------------------------------------------------------------
// X86_64 Opcode Definitions (Real Machine Code)
// -----------------------------------------------------------------------------

namespace Opcodes {
    // Movement Instructions
    constexpr uint8_t MOV_REG_IMM64[]  = { 0x48, 0xB8 };  // mov rax, imm64
    constexpr uint8_t MOV_REG_REG[]    = { 0x48, 0x89 };  // mov r/m64, r64
    constexpr uint8_t MOV_MEM_REG[]    = { 0x48, 0x89 };  // mov [mem], reg
    constexpr uint8_t MOV_REG_MEM[]    = { 0x48, 0x8B };  // mov reg, [mem]
    
    // Arithmetic Instructions
    constexpr uint8_t ADD_REG_REG[]    = { 0x48, 0x01 };  // add r/m64, r64
    constexpr uint8_t ADD_REG_IMM32[]  = { 0x48, 0x81, 0xC0 }; // add rax, imm32
    constexpr uint8_t SUB_REG_REG[]    = { 0x48, 0x29 };  // sub r/m64, r64
    constexpr uint8_t SUB_REG_IMM32[]  = { 0x48, 0x81, 0xE8 }; // sub rax, imm32
    constexpr uint8_t IMUL_REG_REG[]   = { 0x48, 0x0F, 0xAF }; // imul r64, r/m64
    constexpr uint8_t IDIV_REG[]       = { 0x48, 0xF7, 0xF8 }; // idiv r/m64
    
    // Comparison and Logical
    constexpr uint8_t CMP_REG_REG[]    = { 0x48, 0x39 };  // cmp r/m64, r64
    constexpr uint8_t CMP_REG_IMM32[]  = { 0x48, 0x81, 0xF8 }; // cmp rax, imm32
    constexpr uint8_t TEST_REG_REG[]   = { 0x48, 0x85 };  // test r/m64, r64
    constexpr uint8_t AND_REG_REG[]    = { 0x48, 0x21 };  // and r/m64, r64
    constexpr uint8_t OR_REG_REG[]     = { 0x48, 0x09 };  // or r/m64, r64
    constexpr uint8_t XOR_REG_REG[]    = { 0x48, 0x31 };  // xor r/m64, r64
    
    // Stack Operations
    constexpr uint8_t PUSH_REG[]       = { 0x50 };        // push r64 (+ reg)
    constexpr uint8_t POP_REG[]        = { 0x58 };        // pop r64 (+ reg)
    constexpr uint8_t PUSH_IMM32[]     = { 0x68 };        // push imm32
    
    // Control Flow
    constexpr uint8_t CALL_REL32[]     = { 0xE8 };        // call rel32
    constexpr uint8_t CALL_REG[]       = { 0xFF, 0xD0 };  // call rax
    constexpr uint8_t RET[]            = { 0xC3 };        // ret
    constexpr uint8_t JMP_REL32[]      = { 0xE9 };        // jmp rel32
    constexpr uint8_t JMP_REG[]        = { 0xFF, 0xE0 };  // jmp rax
    
    // Conditional Jumps
    constexpr uint8_t JE_REL32[]       = { 0x0F, 0x84 };  // je/jz rel32
    constexpr uint8_t JNE_REL32[]      = { 0x0F, 0x85 };  // jne/jnz rel32
    constexpr uint8_t JL_REL32[]       = { 0x0F, 0x8C };  // jl rel32
    constexpr uint8_t JLE_REL32[]      = { 0x0F, 0x8E };  // jle rel32
    constexpr uint8_t JG_REL32[]       = { 0x0F, 0x8F };  // jg rel32
    constexpr uint8_t JGE_REL32[]      = { 0x0F, 0x8D };  // jge rel32
    
    // Special
    constexpr uint8_t NOP[]            = { 0x90 };        // nop
    constexpr uint8_t INT3[]           = { 0xCC };        // int3 (breakpoint)
    constexpr uint8_t SYSCALL[]        = { 0x0F, 0x05 };  // syscall
    
    // LEA (Load Effective Address)
    constexpr uint8_t LEA_REG_MEM[]    = { 0x48, 0x8D };  // lea r64, [mem]
}

// -----------------------------------------------------------------------------
// Windows x64 Calling Convention
// -----------------------------------------------------------------------------

struct WindowsX64CallConv {
    // Parameter registers (in order)
    static constexpr X64Register paramRegs[] = {
        X64Register::RCX,  // 1st parameter
        X64Register::RDX,  // 2nd parameter
        X64Register::R8,   // 3rd parameter
        X64Register::R9    // 4th parameter
    };
    
    // Return value register
    static constexpr X64Register returnReg = X64Register::RAX;
    
    // Scratch registers (caller-saved)
    static constexpr X64Register scratchRegs[] = {
        X64Register::RAX, X64Register::RCX, X64Register::RDX,
        X64Register::R8,  X64Register::R9,  X64Register::R10, X64Register::R11
    };
    
    // Preserved registers (callee-saved)
    static constexpr X64Register preservedRegs[] = {
        X64Register::RBX, X64Register::RBP, X64Register::RDI,
        X64Register::RSI, X64Register::R12, X64Register::R13,
        X64Register::R14, X64Register::R15
    };
    
    // Shadow space size (32 bytes)
    static constexpr size_t SHADOW_SPACE = 32;
    
    // Stack alignment (16 bytes)
    static constexpr size_t STACK_ALIGNMENT = 16;
};

// -----------------------------------------------------------------------------
// Code Buffer for Binary Emission
// -----------------------------------------------------------------------------

class CodeBuffer {
    std::vector<uint8_t> buffer;
    std::unordered_map<std::string, size_t> labels;
    std::vector<std::pair<size_t, std::string>> labelReferences;
    
public:
    CodeBuffer() = default;
    
    // Emit single byte
    void emit8(uint8_t byte) {
        buffer.push_back(byte);
    }
    
    // Emit 16-bit value (little endian)
    void emit16(uint16_t value) {
        buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }
    
    // Emit 32-bit value (little endian)
    void emit32(uint32_t value) {
        buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }
    
    // Emit 64-bit value (little endian)
    void emit64(uint64_t value) {
        for (int i = 0; i < 8; i++) {
            buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }
    
    // Emit byte array
    void emitBytes(const uint8_t* bytes, size_t count) {
        for (size_t i = 0; i < count; i++) {
            buffer.push_back(bytes[i]);
        }
    }
    
    // Label management
    void defineLabel(const std::string& label) {
        labels[label] = buffer.size();
    }
    
    void referenceLabel(const std::string& label) {
        labelReferences.push_back({buffer.size(), label});
        emit32(0); // Placeholder
    }
    
    // Resolve all label references
    void resolveLabels() {
        for (const auto& ref : labelReferences) {
            size_t refPos = ref.first;
            const std::string& labelName = ref.second;
            
            auto it = labels.find(labelName);
            if (it != labels.end()) {
                size_t targetPos = it->second;
                int32_t offset = static_cast<int32_t>(targetPos - (refPos + 4));
                
                // Patch the offset
                buffer[refPos] = static_cast<uint8_t>(offset & 0xFF);
                buffer[refPos + 1] = static_cast<uint8_t>((offset >> 8) & 0xFF);
                buffer[refPos + 2] = static_cast<uint8_t>((offset >> 16) & 0xFF);
                buffer[refPos + 3] = static_cast<uint8_t>((offset >> 24) & 0xFF);
            }
        }
    }
    
    // Get current position
    size_t getPosition() const { return buffer.size(); }
    
    // Get buffer
    const std::vector<uint8_t>& getBuffer() const { return buffer; }
    
    // Clear buffer
    void clear() {
        buffer.clear();
        labels.clear();
        labelReferences.clear();
    }
    
    // Alignment
    void align(size_t alignment) {
        while (buffer.size() % alignment != 0) {
            emit8(0x90); // NOP for padding
        }
    }
};

// -----------------------------------------------------------------------------
// CASE Keyword to Opcode Mapping
// -----------------------------------------------------------------------------

struct CASEOpcodeMapping {
    // Arithmetic operations
    static const uint8_t* getArithmeticOpcode(const std::string& op) {
        if (op == "add" || op == "+") return Opcodes::ADD_REG_REG;
        if (op == "sub" || op == "-") return Opcodes::SUB_REG_REG;
        if (op == "mul" || op == "*") return Opcodes::IMUL_REG_REG;
        if (op == "div" || op == "/") return Opcodes::IDIV_REG;
        return nullptr;
    }
    
    // Comparison operations
    static const uint8_t* getComparisonOpcode(const std::string& op) {
        if (op == "cmp" || op == "==") return Opcodes::CMP_REG_REG;
        if (op == "test") return Opcodes::TEST_REG_REG;
        return Opcodes::CMP_REG_REG;
    }
    
    // Jump operations based on condition
    static const uint8_t* getJumpOpcode(const std::string& condition) {
        if (condition == "eq" || condition == "==") return Opcodes::JE_REL32;
        if (condition == "ne" || condition == "!=") return Opcodes::JNE_REL32;
        if (condition == "lt" || condition == "<")  return Opcodes::JL_REL32;
        if (condition == "le" || condition == "<=") return Opcodes::JLE_REL32;
        if (condition == "gt" || condition == ">")  return Opcodes::JG_REL32;
        if (condition == "ge" || condition == ">=") return Opcodes::JGE_REL32;
        return Opcodes::JMP_REL32;
    }
};

// -----------------------------------------------------------------------------
// Native Code Emitter
// -----------------------------------------------------------------------------

class NativeCodeEmitter {
    CodeBuffer code;
    std::unordered_map<std::string, X64Register> virtualToPhysicalReg;
    std::unordered_map<std::string, int32_t> variableOffsets;
    int32_t currentStackOffset;
    
public:
    NativeCodeEmitter() : currentStackOffset(0) {
        initializeRegisterMapping();
    }
    
    // Emit function prologue
    void emitPrologue() {
        // push rbp
        code.emit8(Opcodes::PUSH_REG[0] + static_cast<uint8_t>(X64Register::RBP));
        
        // mov rbp, rsp
        code.emitBytes(Opcodes::MOV_REG_REG, 2);
        code.emit8(0xEC); // ModR/M: rbp, rsp
        
        // Allocate shadow space and local variables
        if (currentStackOffset > 0) {
            // sub rsp, offset
            code.emitBytes(Opcodes::SUB_REG_IMM32, 3);
            code.emit32(currentStackOffset);
        }
    }
    
    // Emit function epilogue
    void emitEpilogue() {
        // mov rsp, rbp
        code.emitBytes(Opcodes::MOV_REG_REG, 2);
        code.emit8(0xE5); // ModR/M: rsp, rbp
        
        // pop rbp
        code.emit8(Opcodes::POP_REG[0] + static_cast<uint8_t>(X64Register::RBP));
        
        // ret
        code.emit8(Opcodes::RET[0]);
    }
    
    // Emit MOV instruction
    void emitMov(X64Register dest, X64Register src) {
        code.emitBytes(Opcodes::MOV_REG_REG, 2);
        code.emit8(makeModRM(3, static_cast<uint8_t>(src), static_cast<uint8_t>(dest)));
    }
    
    void emitMovImm(X64Register dest, int64_t immediate) {
        // mov reg, imm64
        code.emit8(0x48); // REX.W
        code.emit8(0xB8 + static_cast<uint8_t>(dest));
        code.emit64(static_cast<uint64_t>(immediate));
    }
    
    // Emit ADD instruction
    void emitAdd(X64Register dest, X64Register src) {
        code.emitBytes(Opcodes::ADD_REG_REG, 2);
        code.emit8(makeModRM(3, static_cast<uint8_t>(src), static_cast<uint8_t>(dest)));
    }
    
    // Emit SUB instruction
    void emitSub(X64Register dest, X64Register src) {
        code.emitBytes(Opcodes::SUB_REG_REG, 2);
        code.emit8(makeModRM(3, static_cast<uint8_t>(src), static_cast<uint8_t>(dest)));
    }
    
    // Emit MUL instruction
    void emitMul(X64Register dest, X64Register src) {
        code.emitBytes(Opcodes::IMUL_REG_REG, 3);
        code.emit8(makeModRM(3, static_cast<uint8_t>(dest), static_cast<uint8_t>(src)));
    }
    
    // Emit CMP instruction
    void emitCmp(X64Register op1, X64Register op2) {
        code.emitBytes(Opcodes::CMP_REG_REG, 2);
        code.emit8(makeModRM(3, static_cast<uint8_t>(op2), static_cast<uint8_t>(op1)));
    }
    
    // Emit conditional jump
    void emitJumpIf(const std::string& condition, const std::string& label) {
        const uint8_t* opcode = CASEOpcodeMapping::getJumpOpcode(condition);
        code.emitBytes(opcode, 2);
        code.referenceLabel(label);
    }
    
    // Emit unconditional jump
    void emitJump(const std::string& label) {
        code.emit8(Opcodes::JMP_REL32[0]);
        code.referenceLabel(label);
    }
    
    // Emit function call
    void emitCall(const std::string& label) {
        code.emit8(Opcodes::CALL_REL32[0]);
        code.referenceLabel(label);
    }
    
    // Emit Windows API call (GetStdHandle, WriteFile, etc.)
    void emitWindowsAPICall(const std::string& apiName, const std::vector<int64_t>& params) {
        // Set up parameters in Windows x64 calling convention
        for (size_t i = 0; i < params.size() && i < 4; i++) {
            emitMovImm(WindowsX64CallConv::paramRegs[i], params[i]);
        }
        
        // Allocate shadow space
        code.emitBytes(Opcodes::SUB_REG_IMM32, 3);
        code.emit32(WindowsX64CallConv::SHADOW_SPACE);
        
        // Call the API (address needs to be resolved at link time)
        emitCall(apiName);
        
        // Clean up shadow space
        code.emitBytes(Opcodes::ADD_REG_IMM32, 3);
        code.emit32(WindowsX64CallConv::SHADOW_SPACE);
    }
    
    // Emit Print statement (translates to Windows API call)
    void emitPrint(const std::string& message) {
        // Store message in data section (to be implemented)
        // GetStdHandle(STD_OUTPUT_HANDLE)
        emitMovImm(X64Register::RCX, -11); // STD_OUTPUT_HANDLE
        emitCall("GetStdHandle");
        
        // WriteFile call would follow
        // This is simplified - full implementation would handle string data
    }
    
    // Label management
    void defineLabel(const std::string& label) {
        code.defineLabel(label);
    }
    
    // Get code buffer
    const CodeBuffer& getCodeBuffer() const { return code; }
    
    // Finalize code generation
    void finalize() {
        code.resolveLabels();
    }
    
private:
    // Make ModR/M byte
    uint8_t makeModRM(uint8_t mod, uint8_t reg, uint8_t rm) {
        return ((mod & 0x3) << 6) | ((reg & 0x7) << 3) | (rm & 0x7);
    }
    
    // Initialize virtual-to-physical register mapping
    void initializeRegisterMapping() {
        virtualToPhysicalReg["%t0"] = X64Register::RAX;
        virtualToPhysicalReg["%t1"] = X64Register::RCX;
        virtualToPhysicalReg["%t2"] = X64Register::RDX;
        virtualToPhysicalReg["%t3"] = X64Register::RBX;
        virtualToPhysicalReg["%t4"] = X64Register::R8;
        virtualToPhysicalReg["%t5"] = X64Register::R9;
        virtualToPhysicalReg["%t6"] = X64Register::R10;
        virtualToPhysicalReg["%t7"] = X64Register::R11;
    }
    
    // Allocate stack space for variable
    int32_t allocateStackSpace(size_t size) {
        currentStackOffset += size;
        return -currentStackOffset;
    }
};

} // namespace Native
} // namespace CASE
