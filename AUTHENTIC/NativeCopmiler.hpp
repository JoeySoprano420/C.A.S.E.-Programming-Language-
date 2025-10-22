//=============================================================================
//  Violet Aura Creations — Native Compilation Pipeline
//  CIAM -> Optimized Machine Code -> PE/ELF Executable
//=============================================================================

#ifndef NATIVE_COMPILER_HPP
#define NATIVE_COMPILER_HPP

#pragma once
#include "AST.hpp"
#include "MachineCodeEmitter.hpp"
#include "BinaryEmitter.hpp"
#include "OptimizationEngine.hpp"
#include "intelligence.hpp"
#include <string>
#include <vector>
#include <memory>

// -----------------------------------------------------------------------------
// Complete Native Compilation Pipeline
// -----------------------------------------------------------------------------

class NativeCompiler {
public:
 NativeCompiler();
    
    // Main compilation entry point
    bool compile(const std::string& sourceCode, 
      const std::string& outputFilename,
       int optimizationLevel = 2);
    
    // Configuration
    void setOptimizationLevel(int level) { optimizationLevel = level; }
    void enablePGO(const std::string& profileData);
    void enableLTO() { ltoEnabled = true; }
    void setTargetArch(const std::string& arch) { targetArch = arch; }
    
    // Get compilation statistics
    struct CompilationStats {
 size_t originalCodeSize;
        size_t optimizedCodeSize;
    size_t machineCodeSize;
        size_t executableSize;
        double compilationTime;
    int optimizationPasses;
    };
    
    const CompilationStats& getStats() const { return stats; }
    
private:
    int optimizationLevel;
    bool ltoEnabled;
    bool pgoEnabled;
    std::string targetArch;
    std::string profileData;
    
    CompilationStats stats;
    
    // Compilation stages
    std::string preprocessCIAM(const std::string& source);
    NodePtr parse(const std::string& source);
    NodePtr optimize(NodePtr ast);
    std::vector<uint8_t> generateMachineCode(NodePtr ast);
    bool linkAndEmit(const std::vector<uint8_t>& code, const std::string& filename);
    
    // Windows-specific linking
    bool linkWindows(const std::vector<uint8_t>& code, 
              const std::vector<uint8_t>& data,
             const std::string& filename);
    
    // Helper: Add runtime library stubs
    std::vector<uint8_t> addRuntimeStubs(const std::vector<uint8_t>& code);
    std::vector<uint8_t> createDataSection();
};

#endif // NATIVE_COMPILER_HPP
