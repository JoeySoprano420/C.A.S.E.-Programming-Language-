//=============================================================================
//  Violet Aura Creations — Machine Code Emitter Implementation
//  Direct x86-64 machine code generation
//=============================================================================

#include "MachineCodeEmitter.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

std::vector<uint8_t> MachineCodeEmitter::emit(NodePtr root) {
    std::cout << "\n\033[1;35m[CIAM AOT]\033[0m Direct machine code emission started\n";
    
    // Emit entry point
  section.emitLabel("_start");
    
    // Emit prologue
    section.emitBytes(CIAM::X64Builder::PUSH_REG(CIAM::Reg::RBP).bytes);
    auto movInst = CIAM::X64Builder::MOV_REG_IMM(CIAM::Reg::RBP, 0);
    // MOV RBP, RSP equivalent (simplified)
    section.emitBytes({0x48, 0x89, 0xE5});  // mov rbp, rsp
    
  // Emit main code
    emitNode(root);
    
    // Emit exit syscall
    emitSystemExit(0);
    
    std::cout << "\033[1;35m[CIAM AOT]\033[0m Generated " << section.code.size() 
        << " bytes of machine code\n";
    
    return section.code;
}

void MachineCodeEmitter::emitNode(NodePtr node) {
    if (auto block = std::dynamic_pointer_cast<Block>(node)) {
 for (auto& stmt : block->statements) {
    emitNode(stmt);
        }
    }
    else if (auto print = std::dynamic_pointer_cast<PrintStmt>(node)) {
        emitPrint(print->expr);
    }
    else if (auto varDecl = std::dynamic_pointer_cast<VarDecl>(node)) {
        emitVarDecl(varDecl->name, varDecl->initializer);
    }
    else if (auto fn = std::dynamic_pointer_cast<FunctionDecl>(node)) {
 emitFunction(fn->name, fn->body);
    }
    else if (auto ret = std::dynamic_pointer_cast<ReturnStmt>(node)) {
        emitReturn(ret->value);
    }
    else if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(node)) {
 emitIfStmt(ifStmt->condition, ifStmt->thenBlock, ifStmt->elseBlock);
    }
    else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(node)) {
        emitWhileStmt(whileStmt->condition, whileStmt->block);
    }
    else if (auto call = std::dynamic_pointer_cast<CallExpr>(node)) {
        // Emit function call
      section.addRelocation(call->callee);
     section.emitBytes(CIAM::X64Builder::CALL_REL32(0).bytes);
    }
    // Add more node types as needed
}

CIAM::Reg MachineCodeEmitter::emitExpr(NodePtr expr) {
    if (auto lit = std::dynamic_pointer_cast<Literal>(expr)) {
        // Parse literal value
        uint64_t value = 0;
        try {
       value = std::stoull(lit->value);
        } catch (...) {
            // Handle string literals differently
            value = 0;
        }
        
        CIAM::Reg reg = regAlloc.allocate("_temp" + std::to_string(labelCounter++));
    section.emitBytes(CIAM::X64Builder::MOV_REG_IMM(reg, value).bytes);
        return reg;
    }
    else if (auto id = std::dynamic_pointer_cast<Identifier>(expr)) {
  return regAlloc.getReg(id->name);
    }
    else if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        CIAM::Reg left = emitExpr(bin->left);
CIAM::Reg right = emitExpr(bin->right);
        
  if (bin->op == "+") {
    section.emitBytes(CIAM::X64Builder::ADD_REG_REG(left, right).bytes);
        }
      else if (bin->op == "-") {
            section.emitBytes(CIAM::X64Builder::SUB_REG_REG(left, right).bytes);
        }
        else if (bin->op == "*") {
          section.emitBytes(CIAM::X64Builder::IMUL_REG_REG(left, right).bytes);
        }
      
     regAlloc.free(right);
        return left;
    }
    
    return CIAM::Reg::RAX;
}

void MachineCodeEmitter::emitPrint(NodePtr expr) {
    if (auto lit = std::dynamic_pointer_cast<Literal>(expr)) {
        // String literal
      if (lit->value.front() == '"') {
            std::string str = lit->value.substr(1, lit->value.length() - 2);
            emitPrintString(str);
 return;
        }
    }
    
    // Numeric expression
    CIAM::Reg reg = emitExpr(expr);
    emitPrintNumber(reg);
    regAlloc.free(reg);
}

void MachineCodeEmitter::emitPrintString(const std::string& str) {
    // Add string to data section
    uint32_t dataOffset = static_cast<uint32_t>(section.data.size());
    section.data.insert(section.data.end(), str.begin(), str.end());
    section.data.push_back('\n');
    section.data.push_back(0);
    
#ifdef _WIN32
    // Windows: WriteFile system call (simplified)
    // This would require full Windows API integration
    std::cout << "\033[1;33m[CIAM]\033[0m Windows print stub (string at data+" 
  << dataOffset << ")\n";
#else
    // Linux: sys_write syscall
    // mov rax, 1 (sys_write)
    section.emitBytes(CIAM::X64Builder::MOV_REG_IMM(CIAM::Reg::RAX, 1).bytes);
    
    // mov rdi, 1 (stdout)
    section.emitBytes(CIAM::X64Builder::MOV_REG_IMM(CIAM::Reg::RDI, 1).bytes);
    
    // lea rsi, [data + offset]
 section.emitBytes(CIAM::X64Builder::LEA_REG_MEM(CIAM::Reg::RSI, CIAM::Reg::RIP, dataOffset).bytes);
    
    // mov rdx, length
    section.emitBytes(CIAM::X64Builder::MOV_REG_IMM(CIAM::Reg::RDX, str.length() + 1).bytes);
    
    // syscall
    section.emitBytes(CIAM::X64Builder::SYSCALL().bytes);
#endif
}

void MachineCodeEmitter::emitPrintNumber(CIAM::Reg reg) {
    // Convert number to string and print
    // This is simplified - full implementation would need conversion routine
    std::cout << "\033[1;33m[CIAM]\033[0m Number print stub (from register)\n";
    
#ifdef _WIN32
    // Windows stub
#else
    // Linux: Convert to ASCII and sys_write
    // For now, just emit placeholder
    section.emitBytes(CIAM::X64Builder::PUSH_REG(reg).bytes);
    // ... conversion code would go here ...
    section.emitBytes(CIAM::X64Builder::POP_REG(reg).bytes);
#endif
}

void MachineCodeEmitter::emitSystemExit(int code) {
#ifdef _WIN32
    // Windows: ExitProcess
    section.emitBytes(CIAM::X64Builder::MOV_REG_IMM(CIAM::Reg::RCX, code).bytes);
    // Call ExitProcess (would need import table)
    std::cout << "\033[1;33m[CIAM]\033[0m Windows exit stub\n";
#else
    // Linux: sys_exit
    section.emitBytes(CIAM::X64Builder::MOV_REG_IMM(CIAM::Reg::RAX, 60).bytes);  // sys_exit
    section.emitBytes(CIAM::X64Builder::MOV_REG_IMM(CIAM::Reg::RDI, code).bytes); // exit code
    section.emitBytes(CIAM::X64Builder::SYSCALL().bytes);
#endif
}

void MachineCodeEmitter::emitVarDecl(const std::string& name, NodePtr initializer) {
    if (initializer) {
        CIAM::Reg reg = emitExpr(initializer);
        regAlloc.allocate(name);  // Associate variable with register
 } else {
        regAlloc.allocate(name);
    }
}

void MachineCodeEmitter::emitFunction(const std::string& name, NodePtr body) {
    section.emitLabel(name);
    
    // Function prologue
    section.emitBytes(CIAM::X64Builder::PUSH_REG(CIAM::Reg::RBP).bytes);
    section.emitBytes({0x48, 0x89, 0xE5});  // mov rbp, rsp
    
    emitNode(body);
    
    // Function epilogue (if no explicit return)
    section.emitBytes({0x48, 0x89, 0xEC});  // mov rsp, rbp
 section.emitBytes(CIAM::X64Builder::POP_REG(CIAM::Reg::RBP).bytes);
    section.emitBytes(CIAM::X64Builder::RET().bytes);
}

void MachineCodeEmitter::emitReturn(NodePtr value) {
    if (value) {
   CIAM::Reg reg = emitExpr(value);
        if (reg != CIAM::Reg::RAX) {
            // Move result to RAX (standard return register)
         section.emitBytes({0x48, 0x89, 0xC0});  // Simplified mov
        }
    }
    
    // Epilogue
 section.emitBytes({0x48, 0x89, 0xEC});  // mov rsp, rbp
    section.emitBytes(CIAM::X64Builder::POP_REG(CIAM::Reg::RBP).bytes);
    section.emitBytes(CIAM::X64Builder::RET().bytes);
}

void MachineCodeEmitter::emitIfStmt(NodePtr condition, NodePtr thenBlock, NodePtr elseBlock) {
    CIAM::Reg condReg = emitExpr(condition);

    // Compare with zero
    section.emitBytes(CIAM::X64Builder::XOR_REG_REG(CIAM::Reg::R11).bytes);
    section.emitBytes(CIAM::X64Builder::CMP_REG_REG(condReg, CIAM::Reg::R11).bytes);
    
    std::string elseLabel = generateLabel("else");
    std::string endLabel = generateLabel("endif");
    
    // Jump to else if condition is zero (false)
    section.addRelocation(elseLabel);
    section.emitBytes(CIAM::X64Builder::JE_REL32(0).bytes);
    
    // Then block
    emitNode(thenBlock);
    section.addRelocation(endLabel);
    section.emitBytes(CIAM::X64Builder::JMP_REL32(0).bytes);
    
    // Else block
    section.emitLabel(elseLabel);
    if (elseBlock) {
        emitNode(elseBlock);
    }
    
    section.emitLabel(endLabel);
    regAlloc.free(condReg);
}

void MachineCodeEmitter::emitWhileStmt(NodePtr condition, NodePtr block) {
    std::string loopLabel = generateLabel("loop");
    std::string endLabel = generateLabel("endloop");
    
    section.emitLabel(loopLabel);
    
    CIAM::Reg condReg = emitExpr(condition);
    section.emitBytes(CIAM::X64Builder::XOR_REG_REG(CIAM::Reg::R11).bytes);
    section.emitBytes(CIAM::X64Builder::CMP_REG_REG(condReg, CIAM::Reg::R11).bytes);
    
 // Jump to end if condition is false
    section.addRelocation(endLabel);
    section.emitBytes(CIAM::X64Builder::JE_REL32(0).bytes);
  
    // Loop body
    emitNode(block);
    
    // Jump back to loop start
    section.addRelocation(loopLabel);
    section.emitBytes(CIAM::X64Builder::JMP_REL32(0).bytes);
    
    section.emitLabel(endLabel);
    regAlloc.free(condReg);
}
