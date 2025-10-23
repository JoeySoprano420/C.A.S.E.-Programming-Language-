//=============================================================================
//  Violet Aura Creations — Hex-IR (Hexadecimal Intermediate Representation)
//  SSA-based typed intermediate representation with vector metadata
//=============================================================================

#pragma once

#include "AST.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace HexIR {

// =============================================================================
// Type System
// =============================================================================

enum class IRType {
    VOID, BOOL,
  I8, I16, I32, I64,
    U8, U16, U32, U64,
    F32, F64,
    PTR,
    VECTOR,  // SIMD vector type
    STRUCT,
    FUNCTION
};

struct TypeInfo {
    IRType baseType;
    size_t bitWidth;
    size_t vectorWidth;  // For SIMD types (0 = scalar)
    std::string structName;
    std::vector<TypeInfo> structFields;
    
    TypeInfo(IRType type = IRType::VOID, size_t bits = 0, size_t vecWidth = 0)
        : baseType(type), bitWidth(bits), vectorWidth(vecWidth) {}
    
    bool isVector() const { return vectorWidth > 0; }
    bool isInteger() const { 
   return baseType >= IRType::I8 && baseType <= IRType::U64; 
    }
    bool isFloat() const { 
        return baseType == IRType::F32 || baseType == IRType::F64; 
    }
};

// =============================================================================
// SSA Value System
// =============================================================================

class Value {
public:
    virtual ~Value() = default;
    
    uint32_t id;
    TypeInfo type;
    std::string name;
 
    Value(uint32_t id, const TypeInfo& type, const std::string& name = "")
  : id(id), type(type), name(name) {}
};

using ValuePtr = std::shared_ptr<Value>;

class Register : public Value {
public:
    Register(uint32_t id, const TypeInfo& type, const std::string& name = "")
        : Value(id, type, name) {}
};

class Constant : public Value {
public:
    uint64_t intValue;
    double floatValue;
    std::string stringValue;
    
    Constant(uint32_t id, const TypeInfo& type, uint64_t val)
        : Value(id, type), intValue(val), floatValue(0.0) {}
    
    Constant(uint32_t id, const TypeInfo& type, double val)
        : Value(id, type), intValue(0), floatValue(val) {}
};

// =============================================================================
// IR Instructions (SSA Form)
// =============================================================================

enum class OpCode {
    // Arithmetic
    ADD, SUB, MUL, DIV, MOD,
    FADD, FSUB, FMUL, FDIV,
    
    // Logical
    AND, OR, XOR, NOT,
SHL, SHR, SAR,
    
    // Comparison
    EQ, NE, LT, LE, GT, GE,
    
    // Memory
    LOAD, STORE, ALLOCA,
    
    // Control Flow
    BR, CONDBR, SWITCH, RET, CALL,
  
    // SSA
    PHI,
    
    // SIMD
    VADD, VSUB, VMUL, VDIV,
    VLOAD, VSTORE,
    SHUFFLE, BROADCAST,
  
    // Special
    CAST, SELECT, EXTRACT, INSERT,
    
    // Base-12 Arithmetic
    DADD, DSUB, DMUL, DDIV,
    
    // Temporal Sync (Dozisecond)
    TSYNC, TMARK
};

class Instruction {
public:
    OpCode opcode;
    ValuePtr result;
    std::vector<ValuePtr> operands;
    std::unordered_map<std::string, std::string> metadata;
    
    // Debug info
    int sourceLine;
    int sourceColumn;
    std::string sourceFile;
    
    Instruction(OpCode op) 
        : opcode(op), sourceLine(0), sourceColumn(0) {}
    
 void addMetadata(const std::string& key, const std::string& value) {
      metadata[key] = value;
    }
    
    std::string getMetadata(const std::string& key) const {
      auto it = metadata.find(key);
        return (it != metadata.end()) ? it->second : "";
    }
};

using InstructionPtr = std::shared_ptr<Instruction>;

// =============================================================================
// Basic Blocks (CFG)
// =============================================================================

class BasicBlock {
public:
    uint32_t id;
    std::string label;
    std::vector<InstructionPtr> instructions;
    std::vector<BasicBlock*> predecessors;
    std::vector<BasicBlock*> successors;
    
    // Dominator tree info
    BasicBlock* immediateDominator;
    std::vector<BasicBlock*> dominatorChildren;
    
 // Loop info
    bool isLoopHeader;
    BasicBlock* loopPreheader;
    std::vector<BasicBlock*> loopExits;
    
  BasicBlock(uint32_t id, const std::string& label = "")
   : id(id), label(label)
   , immediateDominator(nullptr)
        , isLoopHeader(false)
        , loopPreheader(nullptr) {}
    
    void addInstruction(InstructionPtr inst) {
        instructions.push_back(inst);
    }
    
    void addSuccessor(BasicBlock* bb) {
        successors.push_back(bb);
        bb->predecessors.push_back(this);
    }
};

using BasicBlockPtr = std::shared_ptr<BasicBlock>;

// =============================================================================
// IR Function
// =============================================================================

class Function {
public:
    std::string name;
    TypeInfo returnType;
    std::vector<ValuePtr> parameters;
    std::vector<BasicBlockPtr> basicBlocks;
    BasicBlockPtr entryBlock;
    
    // SSA value factory
    uint32_t nextValueId;
    uint32_t nextBlockId;
    
    Function(const std::string& name, const TypeInfo& retType)
        : name(name), returnType(retType)
        , nextValueId(0), nextBlockId(0)
  , entryBlock(nullptr) {}
    
ValuePtr createRegister(const TypeInfo& type, const std::string& name = "") {
        return std::make_shared<Register>(nextValueId++, type, name);
    }
    
    ValuePtr createConstant(uint64_t val, const TypeInfo& type) {
        return std::make_shared<Constant>(nextValueId++, type, val);
    }
    
    ValuePtr createConstant(double val, const TypeInfo& type) {
        return std::make_shared<Constant>(nextValueId++, type, val);
    }
    
    BasicBlockPtr createBasicBlock(const std::string& label = "") {
 auto bb = std::make_shared<BasicBlock>(nextBlockId++, label);
        basicBlocks.push_back(bb);
        if (!entryBlock) {
       entryBlock = bb;
   }
  return bb;
    }
};

using FunctionPtr = std::shared_ptr<Function>;

// =============================================================================
// IR Module (Compilation Unit)
// =============================================================================

class Module {
public:
    std::string name;
    std::vector<FunctionPtr> functions;
    std::unordered_map<std::string, TypeInfo> structTypes;
    std::unordered_map<std::string, ValuePtr> globalVariables;
    
    // Debug info
    std::string sourceFile;
  std::vector<std::string> sourceLines;
    
    Module(const std::string& name) : name(name) {}
    
  FunctionPtr createFunction(const std::string& name, const TypeInfo& retType) {
        auto func = std::make_shared<Function>(name, retType);
        functions.push_back(func);
    return func;
    }
    
    void addStructType(const std::string& name, const TypeInfo& type) {
      structTypes[name] = type;
    }
    
    void addGlobal(const std::string& name, ValuePtr value) {
   globalVariables[name] = value;
  }
};

using ModulePtr = std::shared_ptr<Module>;

// =============================================================================
// IR Builder (Constructs SSA form)
// =============================================================================

class IRBuilder {
public:
    IRBuilder(FunctionPtr func) 
        : currentFunction(func)
        , currentBlock(nullptr) {}
    
    void setInsertPoint(BasicBlockPtr block) {
  currentBlock = block;
    }
    
// Arithmetic
    ValuePtr createAdd(ValuePtr lhs, ValuePtr rhs) {
        auto result = currentFunction->createRegister(lhs->type);
        auto inst = std::make_shared<Instruction>(OpCode::ADD);
      inst->result = result;
    inst->operands = {lhs, rhs};
        currentBlock->addInstruction(inst);
        return result;
  }
    
    ValuePtr createSub(ValuePtr lhs, ValuePtr rhs) {
        auto result = currentFunction->createRegister(lhs->type);
auto inst = std::make_shared<Instruction>(OpCode::SUB);
        inst->result = result;
        inst->operands = {lhs, rhs};
        currentBlock->addInstruction(inst);
        return result;
    }
    
    ValuePtr createMul(ValuePtr lhs, ValuePtr rhs) {
        auto result = currentFunction->createRegister(lhs->type);
  auto inst = std::make_shared<Instruction>(OpCode::MUL);
      inst->result = result;
 inst->operands = {lhs, rhs};
        currentBlock->addInstruction(inst);
 return result;
    }
    
    ValuePtr createDiv(ValuePtr lhs, ValuePtr rhs) {
        auto result = currentFunction->createRegister(lhs->type);
      auto inst = std::make_shared<Instruction>(OpCode::DIV);
        inst->result = result;
  inst->operands = {lhs, rhs};
        currentBlock->addInstruction(inst);
        return result;
    }
    
  // Comparison
    ValuePtr createCmp(OpCode cmpOp, ValuePtr lhs, ValuePtr rhs) {
        auto result = currentFunction->createRegister(TypeInfo(IRType::BOOL, 1));
        auto inst = std::make_shared<Instruction>(cmpOp);
        inst->result = result;
        inst->operands = {lhs, rhs};
        currentBlock->addInstruction(inst);
        return result;
    }
    
    // Memory
    ValuePtr createLoad(ValuePtr ptr) {
        auto result = currentFunction->createRegister(ptr->type);
    auto inst = std::make_shared<Instruction>(OpCode::LOAD);
        inst->result = result;
        inst->operands = {ptr};
        currentBlock->addInstruction(inst);
     return result;
    }
  
    void createStore(ValuePtr value, ValuePtr ptr) {
        auto inst = std::make_shared<Instruction>(OpCode::STORE);
        inst->operands = {value, ptr};
        currentBlock->addInstruction(inst);
    }
    
    ValuePtr createAlloca(const TypeInfo& type) {
        auto result = currentFunction->createRegister(TypeInfo(IRType::PTR));
        auto inst = std::make_shared<Instruction>(OpCode::ALLOCA);
   inst->result = result;
        currentBlock->addInstruction(inst);
        return result;
    }
    
    // Control Flow
    void createBr(BasicBlockPtr dest) {
        auto inst = std::make_shared<Instruction>(OpCode::BR);
        currentBlock->addInstruction(inst);
        currentBlock->addSuccessor(dest.get());
    }
    
    void createCondBr(ValuePtr cond, BasicBlockPtr trueBB, BasicBlockPtr falseBB) {
      auto inst = std::make_shared<Instruction>(OpCode::CONDBR);
        inst->operands = {cond};
        currentBlock->addInstruction(inst);
        currentBlock->addSuccessor(trueBB.get());
        currentBlock->addSuccessor(falseBB.get());
    }
    
    void createRet(ValuePtr value = nullptr) {
  auto inst = std::make_shared<Instruction>(OpCode::RET);
        if (value) {
            inst->operands = {value};
     }
        currentBlock->addInstruction(inst);
  }
    
    ValuePtr createCall(FunctionPtr callee, const std::vector<ValuePtr>& args) {
        auto result = currentFunction->createRegister(callee->returnType);
        auto inst = std::make_shared<Instruction>(OpCode::CALL);
      inst->result = result;
        inst->operands = args;
   inst->addMetadata("callee", callee->name);
        currentBlock->addInstruction(inst);
        return result;
    }
    
    // SSA PHI node
    ValuePtr createPhi(const TypeInfo& type, 
    const std::vector<std::pair<ValuePtr, BasicBlock*>>& incomingValues) {
   auto result = currentFunction->createRegister(type);
        auto inst = std::make_shared<Instruction>(OpCode::PHI);
    inst->result = result;
for (auto& pair : incomingValues) {
   inst->operands.push_back(pair.first);
    }
        currentBlock->addInstruction(inst);
        return result;
    }
    
    // SIMD Operations
    ValuePtr createVectorAdd(ValuePtr lhs, ValuePtr rhs) {
    auto result = currentFunction->createRegister(lhs->type);
   auto inst = std::make_shared<Instruction>(OpCode::VADD);
        inst->result = result;
        inst->operands = {lhs, rhs};
        inst->addMetadata("simd", "avx2");
  currentBlock->addInstruction(inst);
        return result;
    }
    
    ValuePtr createBroadcast(ValuePtr scalar, size_t vectorWidth) {
        TypeInfo vecType = scalar->type;
        vecType.vectorWidth = vectorWidth;
        auto result = currentFunction->createRegister(vecType);
   auto inst = std::make_shared<Instruction>(OpCode::BROADCAST);
        inst->result = result;
     inst->operands = {scalar};
        currentBlock->addInstruction(inst);
        return result;
    }
    
private:
    FunctionPtr currentFunction;
    BasicBlockPtr currentBlock;
};

// =============================================================================
// IR Verification & Validation
// =============================================================================

class IRVerifier {
public:
    static bool verify(ModulePtr module, std::string& errorMsg);
    static bool verifyFunction(FunctionPtr func, std::string& errorMsg);
    static bool verifySSA(FunctionPtr func, std::string& errorMsg);
    static bool verifyDominatorTree(FunctionPtr func, std::string& errorMsg);
};

} // namespace HexIR
