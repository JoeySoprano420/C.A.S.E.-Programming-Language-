//=============================================================================
//  Violet Aura Creations — Multi-Tier Optimization Engine
//  3-Tier optimization system: Basic (-O1), Aggressive (-O2), Maximum (-O3)
//=============================================================================

#pragma once

#include "HexIR.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace Optimization {

// =============================================================================
// Optimization Statistics
// =============================================================================

struct OptimizationStats {
    // Tier 1 stats
    int constantsFolded;
    int deadCodeEliminated;
    int peepholesApplied;
    int boundsChecksEliminated;
    int branchesOptimized;
    size_t footprintReduction;
    
    // Tier 2 stats
    int loopsUnrolled;
    int loopsFused;
    int vectorizationsApplied;
    int lookaheadMerges;
    int tailCallsEliminated;
    
    // Tier 3 stats
    int pgoOptimizations;
    int ltoOptimizations;
    int autofdo Samples;
    int adaptiveTunings;
    int base12Fusions;
int temporalSyncs;
    int speculativeSchedules;
    
    OptimizationStats()
      : constantsFolded(0), deadCodeEliminated(0), peepholesApplied(0)
        , boundsChecksEliminated(0), branchesOptimized(0), footprintReduction(0)
      , loopsUnrolled(0), loopsFused(0), vectorizationsApplied(0)
     , lookaheadMerges(0), tailCallsEliminated(0)
        , pgoOptimizations(0), ltoOptimizations(0), autofdo Samples(0)
        , adaptiveTunings(0), base12Fusions(0), temporalSyncs(0)
    , speculativeSchedules(0)
    {}
    
    void print() const;
};

// =============================================================================
// TIER 1: Basic Optimizations (-O1)
// =============================================================================

class Tier1Optimizer {
public:
    // Constant folding and propagation
    static bool constantFolding(HexIR::ModulePtr module, OptimizationStats& stats);
 
    // Dead code elimination
    static bool deadCodeElimination(HexIR::ModulePtr module, OptimizationStats& stats);
  
    // Peephole optimizations
    static bool peepholeOptimization(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // Bounds check elimination
    static bool boundsCheckElimination(HexIR::ModulePtr module, OptimizationStats& stats);
  
    // Branch optimization
    static bool branchOptimization(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // Footprint compression
    static bool footprintCompression(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // Run all Tier 1 passes
    static void runAll(HexIR::ModulePtr module, OptimizationStats& stats);
};

// =============================================================================
// TIER 2: Aggressive Optimizations (-O2)
// =============================================================================

class Tier2Optimizer {
public:
    // Loop unrolling
    static bool loopUnrolling(HexIR::ModulePtr module, int unrollFactor, OptimizationStats& stats);
    
    // Loop fusion/fission
    static bool loopFusion(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // Vectorization (SIMD/AVX)
    static bool vectorization(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // Lookahead reordering
    static bool lookaheadReordering(HexIR::ModulePtr module, int depth, OptimizationStats& stats);
    
    // Tail call elimination
    static bool tailCallElimination(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // Run all Tier 2 passes
    static void runAll(HexIR::ModulePtr module, OptimizationStats& stats);
};

// =============================================================================
// TIER 3: Maximum Optimizations (-O3)
// =============================================================================

class Tier3Optimizer {
public:
    // Profile-Guided Optimization
    static bool profileGuidedOptimization(HexIR::ModulePtr module, 
      const std::string& profileData,
           OptimizationStats& stats);
    
    // Link-Time Optimization
    static bool linkTimeOptimization(std::vector<HexIR::ModulePtr>& modules,
     OptimizationStats& stats);
    
    // AutoFDO (Automatic Feedback-Directed Optimization)
    static bool autoFDO(HexIR::ModulePtr module,
   const std::unordered_map<std::string, uint64_t>& counters,
       OptimizationStats& stats);
    
 // Adaptive tuning
    static bool adaptiveTuning(HexIR::ModulePtr module,
                    const std::string& targetCPU,
 OptimizationStats& stats);
    
    // Base-12 arithmetic fusion
 static bool base12Fusion(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // Dozisecond temporal synchronization
    static bool temporalSync(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // Speculative scheduling
    static bool speculativeScheduling(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // Run all Tier 3 passes
    static void runAll(HexIR::ModulePtr module, OptimizationStats& stats);
};

// =============================================================================
// Interprocedural Analysis Layer
// =============================================================================

class InterproceduralAnalysis {
public:
    // Alias analysis
    struct AliasInfo {
        std::unordered_map<HexIR::ValuePtr, std::unordered_set<HexIR::ValuePtr>> aliases;
    };
  static AliasInfo performAliasAnalysis(HexIR::ModulePtr module);
    
    // Escape analysis
    struct EscapeInfo {
        std::unordered_set<HexIR::ValuePtr> escapingValues;
    };
    static EscapeInfo performEscapeAnalysis(HexIR::ModulePtr module);
    
    // Global Value Numbering
    static bool globalValueNumbering(HexIR::ModulePtr module, OptimizationStats& stats);
 
    // Control Flow Graph pruning
    static bool cfgPruning(HexIR::ModulePtr module, OptimizationStats& stats);
    
    // SSA reconstruction
    static void rebuildSSA(HexIR::FunctionPtr function);
    
    // Devirtualization
    static bool devirtualization(HexIR::ModulePtr module, OptimizationStats& stats);
    
 // Cache coloring
    static bool cacheColoring(HexIR::ModulePtr module, OptimizationStats& stats);
 
    // Branch prediction modeling
    struct BranchPredictionModel {
    std::unordered_map<HexIR::BasicBlock*, double> takenProbability;
  };
    static BranchPredictionModel buildBranchModel(HexIR::FunctionPtr function);
};

// =============================================================================
// Master Optimization Pipeline
// =============================================================================

class OptimizationPipeline {
public:
    enum class Level {
      O0,  // No optimization
        O1,  // Basic
        O2,  // Aggressive
        O3   // Maximum
    };
  
    struct Configuration {
        Level level;
 int unrollFactor;
 int lookaheadDepth;
        int passes;
        bool enablePGO;
        bool enableLTO;
        bool enableAutoFDO;
        bool enableAdaptive;
 bool enableBase12;
bool enableTemporal;
        bool enableSpeculative;
        std::string profileDataPath;
        std::string targetCPU;
 
        Configuration()
         : level(Level::O2)
            , unrollFactor(8)
            , lookaheadDepth(5)
            , passes(3)
    , enablePGO(false)
            , enableLTO(false)
       , enableAutoFDO(false)
 , enableAdaptive(true)
            , enableBase12(false)
            , enableTemporal(false)
            , enableSpeculative(false)
  , targetCPU("native")
        {}
    };
    
  OptimizationPipeline(const Configuration& config)
        : config(config) {}
    
    // Optimize single module
    void optimize(HexIR::ModulePtr module);
    
    // Optimize multiple modules with LTO
    void optimizeWithLTO(std::vector<HexIR::ModulePtr>& modules);
    
    // Get optimization statistics
    const OptimizationStats& getStats() const { return stats; }
    
    // Print optimization report
    void printReport() const;
    
private:
    Configuration config;
OptimizationStats stats;
    
    void runTier1(HexIR::ModulePtr module);
    void runTier2(HexIR::ModulePtr module);
    void runTier3(HexIR::ModulePtr module);
    void runInterprocedural(HexIR::ModulePtr module);
};

// =============================================================================
// Profile Data Management
// =============================================================================

class ProfileDataManager {
public:
    struct ProfileEntry {
        std::string functionName;
std::string basicBlockLabel;
        uint64_t executionCount;
        uint64_t cycleCount;
        double branchTaken;
    };
    
    // Load profile data from file
    static bool loadProfile(const std::string& filename,
        std::vector<ProfileEntry>& entries);
    
    // Save profile data to file
    static bool saveProfile(const std::string& filename,
     const std::vector<ProfileEntry>& entries);
    
    // Merge multiple profile runs
    static void mergeProfiles(std::vector<ProfileEntry>& base,
      const std::vector<ProfileEntry>& additional);
    
    // Extract hot functions
    static std::vector<std::string> getHotFunctions(
        const std::vector<ProfileEntry>& profile,
      double threshold = 0.9);
    
    // Extract hot paths
    static std::vector<std::vector<std::string>> getHotPaths(
        const std::vector<ProfileEntry>& profile,
        double threshold = 0.8);
};

// =============================================================================
// AutoFDO Counter System
// =============================================================================

class AutoFDOCounters {
public:
    // Runtime counter structure
    struct Counter {
     uint64_t hitCount;
     uint64_t totalCycles;
 double averageCycles;
        
        Counter() : hitCount(0), totalCycles(0), averageCycles(0.0) {}
    };
  
    // Counter database
    std::unordered_map<std::string, Counter> counters;
    
    // Update counter
    void update(const std::string& location, uint64_t cycles);
    
    // Get counter
    const Counter& get(const std::string& location) const;
    
    // Export to profile data
    std::vector<ProfileDataManager::ProfileEntry> exportProfile() const;
    
    // Import from profile data
    void importProfile(const std::vector<ProfileDataManager::ProfileEntry>& profile);
    
    // Clear all counters
    void clear();
};

// =============================================================================
// Adaptive Tuner
// =============================================================================

class AdaptiveTuner {
public:
    struct HardwareInfo {
        std::string cpuModel;
        int coreCount;
        int threadCount;
        size_t l1CacheSize;
        size_t l2CacheSize;
  size_t l3CacheSize;
      bool hasSSE;
        bool hasAVX;
        bool hasAVX2;
     bool hasAVX512;
        int vectorWidth;  // In bytes

        HardwareInfo()
    : coreCount(1), threadCount(1)
            , l1CacheSize(32*1024), l2CacheSize(256*1024), l3CacheSize(8*1024*1024)
   , hasSSE(true), hasAVX(true), hasAVX2(true), hasAVX512(false)
    , vectorWidth(32)
        {}
    };
    
    // Detect hardware capabilities
    static HardwareInfo detectHardware();
    
    // Tune for specific hardware
    static void tuneForHardware(HexIR::ModulePtr module,
          const HardwareInfo& hw,
   OptimizationStats& stats);
    
    // Tune loop unroll factor based on hardware
 static int suggestUnrollFactor(const HardwareInfo& hw, int loopSize);
    
    // Tune vectorization width
    static int suggestVectorWidth(const HardwareInfo& hw, HexIR::IRType elementType);
    
    // Tune cache blocking
    static std::pair<int, int> suggestCacheBlocking(const HardwareInfo& hw,
         int matrixSize);
};

} // namespace Optimization
