//=============================================================================
//  Violet Aura Creations — Complete AOT Compilation Pipeline
//  CASE → Lex → Parse → HexIR → Multi-Tier Opt → x86-64 → PE + AstroLake Sim
//=============================================================================

#pragma once

#include "AST.hpp"
#include "HexIR.hpp"
#include "MultiTierOptimizer.hpp"
#include "AstroLakeSimulator.hpp"
#include "MachineCodeEmitter.hpp"
#include "BinaryEmitter.hpp"
#include <string>
#include <vector>
#include <memory>

namespace Pipeline {

// =============================================================================
// AST → Hex-IR Lowering
// =============================================================================

class IRLowering {
public:
    // Convert AST to Hex-IR module
    static HexIR::ModulePtr lowerToIR(NodePtr ast, const std::string& moduleName);
    
private:
    static void lowerFunction(std::shared_ptr<FunctionDecl> funcAST,
    HexIR::ModulePtr module);
    
    static void lowerStatement(NodePtr stmt,
      HexIR::IRBuilder& builder,
       HexIR::FunctionPtr function);
    
static HexIR::ValuePtr lowerExpression(NodePtr expr,
         HexIR::IRBuilder& builder,
        HexIR::FunctionPtr function);
};

// =============================================================================
// Complete Compilation Pipeline
// =============================================================================

class CompletePipeline {
public:
    struct Configuration {
        // Output settings
        std::string outputFilename;
        std::string moduleName;
        
        // Optimization settings
        Optimization::OptimizationPipeline::Level optimizationLevel;
int unrollFactor;
        int lookaheadDepth;
    int optimizationPasses;
        
        // PGO settings
bool enablePGO;
        std::string profileDataPath;
        
        // LTO settings
        bool enableLTO;
        std::vector<std::string> additionalModules;
        
        // AutoFDO settings
        bool enableAutoFDO;
        bool enableRuntimeProfiling;
        
        // Adaptive settings
        bool enableAdaptiveTuning;
        std::string targetCPU;
      
        // Advanced settings
        bool enableBase12Arithmetic;
bool enableDoziSecondSync;
        bool enableSpeculativeScheduling;
        
        // AstroLake simulation
        bool enableHardwareSimulation;
        AstroLake::ClusterSimulator::SimulationConfig simConfig;
        
        // Reoptimization
     bool enableAdaptiveReoptimization;
  int reoptimizationRounds;
        
    // Debug/Verbose
  bool verbose;
        bool generateDebugInfo;
        bool dumpIR;
        bool dumpOptimizedIR;
        bool dumpAsm;
   
    Configuration()
      : outputFilename("output.exe")
   , moduleName("main")
       , optimizationLevel(Optimization::OptimizationPipeline::Level::O2)
       , unrollFactor(8)
            , lookaheadDepth(5)
     , optimizationPasses(3)
       , enablePGO(false)
       , enableLTO(false)
            , enableAutoFDO(false)
       , enableRuntimeProfiling(false)
   , enableAdaptiveTuning(true)
, targetCPU("native")
   , enableBase12Arithmetic(false)
            , enableDoziSecondSync(false)
            , enableSpeculativeScheduling(false)
            , enableHardwareSimulation(false)
  , enableAdaptiveReoptimization(false)
     , reoptimizationRounds(0)
  , verbose(true)
         , generateDebugInfo(false)
     , dumpIR(false)
          , dumpOptimizedIR(false)
    , dumpAsm(false)
        {}
    };
    
    struct CompilationResult {
     bool success;
        std::string errorMessage;
        
   // Statistics
        size_t astNodes;
        size_t irInstructions;
size_t optimizedInstructions;
        size_t machineCodeBytes;
     size_t executableSize;
        
        // Timing
        double lexingTime;
 double parsingTime;
   double irGenerationTime;
        double optimizationTime;
        double codeGenTime;
 double linkingTime;
    double simulationTime;
  double totalTime;
      
 // Optimization stats
     Optimization::OptimizationStats optStats;
      
        // Simulation stats (if enabled)
        struct SimulationStats {
            double averageIPC;
            double averageTemperature;
    double peakTemperature;
     double totalPowerUsage;
    double cacheHitRate;
double branchPredictionAccuracy;
        } simStats;
        
    CompilationResult()
            : success(false)
    , astNodes(0), irInstructions(0), optimizedInstructions(0)
            , machineCodeBytes(0), executableSize(0)
    , lexingTime(0.0), parsingTime(0.0), irGenerationTime(0.0)
      , optimizationTime(0.0), codeGenTime(0.0), linkingTime(0.0)
        , simulationTime(0.0), totalTime(0.0)
    {}
    };

    CompletePipeline(const Configuration& config)
  : config(config) {}
    
    // Main compilation entry point
    CompilationResult compile(NodePtr ast);
    
    // Individual pipeline stages
    HexIR::ModulePtr stage1_IRGeneration(NodePtr ast, CompilationResult& result);
    HexIR::ModulePtr stage2_Optimization(HexIR::ModulePtr module, CompilationResult& result);
    std::vector<uint8_t> stage3_CodeGeneration(HexIR::ModulePtr module, CompilationResult& result);
    bool stage4_BinaryEmission(const std::vector<uint8_t>& code, CompilationResult& result);
    void stage5_HardwareSimulation(HexIR::ModulePtr module, CompilationResult& result);
    HexIR::ModulePtr stage6_AdaptiveReoptimization(HexIR::ModulePtr module, 
   const Optimization::AutoFDOCounters& counters,
            CompilationResult& result);
    
    // Print detailed compilation report
    void printReport(const CompilationResult& result) const;
    
private:
    Configuration config;
    
    double getCurrentTime() const;
    void log(const std::string& message) const;
    size_t countInstructions(HexIR::ModulePtr module) const;
};

// =============================================================================
// BubbleRuntime Execution Environment
// =============================================================================

class BubbleRuntime {
public:
    struct RuntimeConfig {
bool enableParallelExecution;
        int maxThreads;
        bool enableCIAMIntrospection;
        bool enableProfilingFeedback;
        std::string feedbackOutputPath;
        
    RuntimeConfig()
     : enableParallelExecution(true)
       , maxThreads(0)  // 0 = auto-detect
    , enableCIAMIntrospection(false)
 , enableProfilingFeedback(false)
        {}
    };
 
    BubbleRuntime(const RuntimeConfig& config)
  : config(config) {}
    
    // Execute compiled binary with profiling
    bool execute(const std::string& executablePath);
    
  // Get runtime profiling data
    Optimization::AutoFDOCounters getProfilingData() const;
    
    // CIAM introspection hooks
    struct CIAMIntrospectionData {
        std::unordered_map<std::string, std::string> typeInfo;
        std::unordered_map<std::string, std::vector<std::string>> callGraph;
     std::unordered_map<std::string, uint64_t> executionCounts;
  };
    
    CIAMIntrospectionData getCIAMData() const;
    
private:
    RuntimeConfig config;
    Optimization::AutoFDOCounters profilingCounters;
    CIAMIntrospectionData ciamData;
};

} // namespace Pipeline
