//=============================================================================
//  Violet Aura Creations — Direct Machine Code Emitter
//  CIAM-Based Pure AOT Compilation (No C++ intermediary)
//=============================================================================

#ifndef MACHINE_CODE_EMITTER_HPP
#define MACHINE_CODE_EMITTER_HPP

#pragma once
#include "AST.hpp"
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>

// -----------------------------------------------------------------------------
// CIAM: Contextual Inference Abstraction Macros
// -----------------------------------------------------------------------------

namespace CIAM {

// CIAM MACRO: Register allocation abstraction
enum class Reg : uint8_t {
    RAX = 0, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
    R8, R9, R10, R11, R12, R13, R14, R15,
    NONE = 0xFF
};

// CIAM MACRO: Instruction encoding abstraction
struct Instruction {
    std::vector<uint8_t> bytes;
    std::string mnemonic;  // For debugging
    
    void emit_byte(uint8_t b) { bytes.push_back(b); }
    void emit_dword(uint32_t d) {
      bytes.push_back(d & 0xFF);
  bytes.push_back((d >> 8) & 0xFF);
        bytes.push_back((d >> 16) & 0xFF);
    bytes.push_back((d >> 24) & 0xFF);
    }
    void emit_qword(uint64_t q) {
        emit_dword(q & 0xFFFFFFFF);
        emit_dword((q >> 32) & 0xFFFFFFFF);
    }
};

// CIAM MACRO: Platform-specific code section abstraction
struct CodeSection {
    std::vector<uint8_t> code;
 std::vector<uint8_t> data;
    std::unordered_map<std::string, uint32_t> labels;
    std::vector<std::pair<uint32_t, std::string>> relocations;
    
    uint32_t currentOffset() const { return static_cast<uint32_t>(code.size()); }
    
    void emitLabel(const std::string& name) {
        labels[name] = currentOffset();
    }
  
    void emitBytes(const std::vector<uint8_t>& bytes) {
        code.insert(code.end(), bytes.begin(), bytes.end());
    }
    
    void addRelocation(const std::string& label) {
        relocations.push_back({currentOffset(), label});
    }
};

// CIAM MACRO: x86-64 instruction builder
class X64Builder {
public:
// CIAM: MOV instruction abstraction
    static Instruction MOV_REG_IMM(Reg dst, uint64_t imm) {
        Instruction inst;
   inst.mnemonic = "mov reg, imm64";
        
        // REX.W prefix for 64-bit
    inst.emit_byte(0x48 | ((static_cast<uint8_t>(dst) >> 3) & 1));
        // MOV opcode + register
        inst.emit_byte(0xB8 | (static_cast<uint8_t>(dst) & 0x7));
        inst.emit_qword(imm);
        
        return inst;
  }
    
    // CIAM: ADD instruction abstraction
    static Instruction ADD_REG_REG(Reg dst, Reg src) {
        Instruction inst;
        inst.mnemonic = "add reg, reg";
        
        // REX.W prefix
        inst.emit_byte(0x48 | 
            ((static_cast<uint8_t>(dst) >> 3) & 1) << 2 |
((static_cast<uint8_t>(src) >> 3) & 1));
        // ADD opcode
        inst.emit_byte(0x01);
        // ModR/M byte
 inst.emit_byte(0xC0 | 
            ((static_cast<uint8_t>(src) & 0x7) << 3) |
            (static_cast<uint8_t>(dst) & 0x7));
    
        return inst;
    }
    
    // CIAM: SUB instruction abstraction
    static Instruction SUB_REG_REG(Reg dst, Reg src) {
        Instruction inst;
   inst.mnemonic = "sub reg, reg";
    
        inst.emit_byte(0x48 | 
            ((static_cast<uint8_t>(dst) >> 3) & 1) << 2 |
            ((static_cast<uint8_t>(src) >> 3) & 1));
        inst.emit_byte(0x29);
        inst.emit_byte(0xC0 | 
         ((static_cast<uint8_t>(src) & 0x7) << 3) |
            (static_cast<uint8_t>(dst) & 0x7));
        
  return inst;
 }
    
    // CIAM: IMUL instruction abstraction
    static Instruction IMUL_REG_REG(Reg dst, Reg src) {
        Instruction inst;
        inst.mnemonic = "imul reg, reg";
        
        inst.emit_byte(0x48 | 
   ((static_cast<uint8_t>(dst) >> 3) & 1) << 2 |
            ((static_cast<uint8_t>(src) >> 3) & 1));
     inst.emit_byte(0x0F);
        inst.emit_byte(0xAF);
        inst.emit_byte(0xC0 | 
            ((static_cast<uint8_t>(dst) & 0x7) << 3) |
        (static_cast<uint8_t>(src) & 0x7));
 
        return inst;
    }
    
    // CIAM: PUSH instruction abstraction
    static Instruction PUSH_REG(Reg reg) {
        Instruction inst;
        inst.mnemonic = "push reg";
        
        if (static_cast<uint8_t>(reg) >= 8) {
   inst.emit_byte(0x41);  // REX.B prefix for R8-R15
        }
        inst.emit_byte(0x50 | (static_cast<uint8_t>(reg) & 0x7));
        
        return inst;
  }
    
    // CIAM: POP instruction abstraction
    static Instruction POP_REG(Reg reg) {
        Instruction inst;
    inst.mnemonic = "pop reg";
        
        if (static_cast<uint8_t>(reg) >= 8) {
  inst.emit_byte(0x41);
        }
        inst.emit_byte(0x58 | (static_cast<uint8_t>(reg) & 0x7));
        
 return inst;
    }
    
    // CIAM: CALL instruction abstraction
    static Instruction CALL_REL32(int32_t offset) {
    Instruction inst;
        inst.mnemonic = "call rel32";
        
    inst.emit_byte(0xE8);
    inst.emit_dword(static_cast<uint32_t>(offset));
        
      return inst;
    }
    
// CIAM: RET instruction abstraction
 static Instruction RET() {
      Instruction inst;
        inst.mnemonic = "ret";
     inst.emit_byte(0xC3);
     return inst;
    }
    
  // CIAM: JMP instruction abstraction
    static Instruction JMP_REL32(int32_t offset) {
        Instruction inst;
 inst.mnemonic = "jmp rel32";
        
        inst.emit_byte(0xE9);
        inst.emit_dword(static_cast<uint32_t>(offset));
        
   return inst;
    }
    
    // CIAM: JE (Jump if Equal) instruction
    static Instruction JE_REL32(int32_t offset) {
        Instruction inst;
        inst.mnemonic = "je rel32";
        
      inst.emit_byte(0x0F);
        inst.emit_byte(0x84);
        inst.emit_dword(static_cast<uint32_t>(offset));
      
        return inst;
  }
    
  // CIAM: CMP instruction abstraction
    static Instruction CMP_REG_REG(Reg left, Reg right) {
        Instruction inst;
        inst.mnemonic = "cmp reg, reg";
        
        inst.emit_byte(0x48 | 
((static_cast<uint8_t>(left) >> 3) & 1) << 2 |
        ((static_cast<uint8_t>(right) >> 3) & 1));
        inst.emit_byte(0x39);
        inst.emit_byte(0xC0 | 
  ((static_cast<uint8_t>(right) & 0x7) << 3) |
    (static_cast<uint8_t>(left) & 0x7));

  return inst;
    }
    
    // CIAM: System call abstraction (Linux x86-64)
    static Instruction SYSCALL() {
        Instruction inst;
  inst.mnemonic = "syscall";
        inst.emit_byte(0x0F);
        inst.emit_byte(0x05);
        return inst;
    }
    
    // CIAM: INT instruction (Windows system calls)
    static Instruction INT_IMM8(uint8_t vector) {
   Instruction inst;
    inst.mnemonic = "int imm8";
     inst.emit_byte(0xCD);
        inst.emit_byte(vector);
        return inst;
    }
 
  // CIAM: XOR for register clearing
    static Instruction XOR_REG_REG(Reg reg) {
        Instruction inst;
  inst.mnemonic = "xor reg, reg";
        
        inst.emit_byte(0x48 | ((static_cast<uint8_t>(reg) >> 3) & 1) << 2);
        inst.emit_byte(0x31);
        inst.emit_byte(0xC0 | 
            ((static_cast<uint8_t>(reg) & 0x7) << 3) |
       (static_cast<uint8_t>(reg) & 0x7));
      
        return inst;
    }
    
    // CIAM: LEA (Load Effective Address)
    static Instruction LEA_REG_MEM(Reg dst, Reg base, int32_t offset) {
        Instruction inst;
        inst.mnemonic = "lea reg, [base + offset]";
    
  inst.emit_byte(0x48 | 
        ((static_cast<uint8_t>(dst) >> 3) & 1) << 2 |
  ((static_cast<uint8_t>(base) >> 3) & 1));
 inst.emit_byte(0x8D);
   
        // ModR/M and SIB bytes for [base + disp32]
        inst.emit_byte(0x80 | 
            ((static_cast<uint8_t>(dst) & 0x7) << 3) |
            (static_cast<uint8_t>(base) & 0x7));
     inst.emit_dword(static_cast<uint32_t>(offset));
  
        return inst;
    }
};

} // namespace CIAM

// -----------------------------------------------------------------------------
// Register Allocator (CIAM-based)
// -----------------------------------------------------------------------------

class RegisterAllocator {
public:
    RegisterAllocator() {
        // Initialize available registers (avoid RSP, RBP for stack operations)
      available = {
CIAM::Reg::RAX, CIAM::Reg::RCX, CIAM::Reg::RDX, CIAM::Reg::RBX,
        CIAM::Reg::RSI, CIAM::Reg::RDI,
            CIAM::Reg::R8, CIAM::Reg::R9, CIAM::Reg::R10, CIAM::Reg::R11,
            CIAM::Reg::R12, CIAM::Reg::R13, CIAM::Reg::R14, CIAM::Reg::R15
        };
    }
    
    CIAM::Reg allocate(const std::string& varName) {
        if (varToReg.count(varName)) {
          return varToReg[varName];
        }
        
      if (available.empty()) {
        // Spill to stack (simplified)
    return CIAM::Reg::NONE;
    }
        
CIAM::Reg reg = available.back();
        available.pop_back();
        varToReg[varName] = reg;
     regToVar[reg] = varName;
        
        return reg;
    }
    
    void free(CIAM::Reg reg) {
        if (regToVar.count(reg)) {
            std::string varName = regToVar[reg];
            varToReg.erase(varName);
            regToVar.erase(reg);
        available.push_back(reg);
        }
    }
    
    void free(const std::string& varName) {
        if (varToReg.count(varName)) {
          CIAM::Reg reg = varToReg[varName];
         free(reg);
        }
    }
    
    CIAM::Reg getReg(const std::string& varName) const {
        auto it = varToReg.find(varName);
        return (it != varToReg.end()) ? it->second : CIAM::Reg::NONE;
    }
    
private:
    std::vector<CIAM::Reg> available;
    std::unordered_map<std::string, CIAM::Reg> varToReg;
    std::unordered_map<CIAM::Reg, std::string> regToVar;
};

// -----------------------------------------------------------------------------
// Machine Code Emitter
// -----------------------------------------------------------------------------

class MachineCodeEmitter {
public:
    MachineCodeEmitter() : labelCounter(0) {}
    
    std::vector<uint8_t> emit(NodePtr root);
    
private:
    CIAM::CodeSection section;
    RegisterAllocator regAlloc;
    int labelCounter;
    
    std::string generateLabel(const std::string& prefix = "L") {
        return prefix + std::to_string(labelCounter++);
    }
    
    void emitNode(NodePtr node);
    CIAM::Reg emitExpr(NodePtr expr);
    
  // Emit helper functions
    void emitPrint(NodePtr expr);
    void emitFunction(const std::string& name, NodePtr body);
    void emitReturn(NodePtr value);
    void emitIfStmt(NodePtr condition, NodePtr thenBlock, NodePtr elseBlock);
    void emitWhileStmt(NodePtr condition, NodePtr block);
    void emitVarDecl(const std::string& name, NodePtr initializer);
 
    // Platform-specific runtime calls
    void emitPrintString(const std::string& str);
    void emitPrintNumber(CIAM::Reg reg);
    void emitSystemExit(int code);
};

#endif // MACHINE_CODE_EMITTER_HPP
