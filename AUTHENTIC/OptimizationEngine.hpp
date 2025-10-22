//=============================================================================
//  Violet Aura Creations — Advanced Optimization Engine
//  Aggressive multi-pass optimization with AI-guided tuning
//=============================================================================

#ifndef OPTIMIZATION_ENGINE_HPP
#define OPTIMIZATION_ENGINE_HPP

#pragma once
#include "AST.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cstdint>
#include <functional>

namespace Optimization {

// -----------------------------------------------------------------------------
// Optimization Context - Tracks environment and user behavior patterns
// -----------------------------------------------------------------------------
struct OptimizationContext {
    // Execution profile data
    std::unordered_map<std::string, uint64_t> functionCallCounts;
 std::unordered_map<std::string, uint64_t> loopIterationCounts;
    std::unordered_map<std::string, double> branchPredictions; // 0.0-1.0
    
    // Resource usage patterns
    std::unordered_map<std::string, size_t> memoryFootprints;
    std::unordered_map<std::string, uint64_t> cpuCycles;
    
    // User tendency detection
    enum class UserPattern {
        ComputeIntensive,
        MemoryIntensive,
        IOIntensive,
        BalancedWorkload
    };
    UserPattern detectedPattern = UserPattern::BalancedWorkload;
    
    // Environment detection
    size_t availableCores = 1;
    size_t cacheLineSize = 64;
    size_t l1CacheSize = 32768;
  size_t l2CacheSize = 262144;
    size_t l3CacheSize = 8388608;
    bool hasAVX2 = false;
    bool hasAVX512 = false;
    bool hasSSE42 = false;
};

// -----------------------------------------------------------------------------
// Dead Code Elimination Pass
// -----------------------------------------------------------------------------
class DeadCodeEliminator {
public:
    NodePtr eliminate(NodePtr root);
    
private:
    std::unordered_set<std::string> usedVariables;
    std::unordered_set<std::string> usedFunctions;
    
    void collectUsedSymbols(NodePtr node);
    bool isNodeUsed(NodePtr node);
    NodePtr eliminateInBlock(NodePtr node);
};

// -----------------------------------------------------------------------------
// Constant Folding & Propagation
// -----------------------------------------------------------------------------
class ConstantFolder {
public:
    NodePtr fold(NodePtr root);
    
private:
    std::unordered_map<std::string, NodePtr> constants;
    
    NodePtr foldExpression(NodePtr expr);
    NodePtr evaluateConstantExpr(NodePtr left, const std::string& op, NodePtr right);
    bool isConstant(NodePtr node);
    int64_t getIntValue(NodePtr node);
};

// -----------------------------------------------------------------------------
// Loop Optimization (Unrolling, Invariant Motion, Strength Reduction)
// -----------------------------------------------------------------------------
class LoopOptimizer {
public:
    LoopOptimizer(OptimizationContext& ctx) : context(ctx) {}
    
    NodePtr optimize(NodePtr root);
    
private:
    OptimizationContext& context;
    
    // Loop unrolling with adaptive factor based on body size
    NodePtr unrollLoop(NodePtr loopNode, int factor = -1);
    
    // Loop-invariant code motion
    NodePtr hoistInvariants(NodePtr loopNode);
    
    // Strength reduction (e.g., multiply -> shift for powers of 2)
    NodePtr strengthReduce(NodePtr loopNode);
    
    // Vectorization preparation
    NodePtr prepareVectorization(NodePtr loopNode);
    
    int determineUnrollFactor(NodePtr loopBody);
    bool canUnroll(NodePtr loopNode);
    std::vector<NodePtr> extractInvariants(NodePtr body, const std::unordered_set<std::string>& loopVars);
};

// -----------------------------------------------------------------------------
// Peephole Optimizer - Local instruction pattern matching
// -----------------------------------------------------------------------------
class PeepholeOptimizer {
public:
    std::vector<uint8_t> optimize(const std::vector<uint8_t>& code);
    
private:
    struct Pattern {
std::vector<uint8_t> match;
        std::vector<uint8_t> replacement;
        std::string description;
    };
    
    std::vector<Pattern> patterns;
    
    void initializePatterns();
    bool matchPattern(const std::vector<uint8_t>& code, size_t offset, const Pattern& pattern);
};

// -----------------------------------------------------------------------------
// Tail Call Optimization
// -----------------------------------------------------------------------------
class TailCallOptimizer {
public:
    NodePtr optimize(NodePtr root);
    
private:
    bool isTailCall(NodePtr node, const std::string& currentFunction);
    NodePtr convertToJump(NodePtr callNode);
};

// -----------------------------------------------------------------------------
// Branch Predictor & Optimizer
// -----------------------------------------------------------------------------
class BranchOptimizer {
public:
    BranchOptimizer(OptimizationContext& ctx) : context(ctx) {}
    
    NodePtr optimize(NodePtr root);
 
private:
    OptimizationContext& context;
    
    // Reorder branches based on prediction data
    NodePtr reorderBranches(NodePtr ifNode);
    
    // Convert unlikely branches to conditional moves
    NodePtr convertToCMov(NodePtr ifNode);
    
    // Profile-guided branch hints
    void addBranchHints(NodePtr ifNode);
};

// -----------------------------------------------------------------------------
// Vectorization Engine (SIMD)
// -----------------------------------------------------------------------------
class Vectorizer {
public:
    Vectorizer(OptimizationContext& ctx) : context(ctx) {}
    
    NodePtr vectorize(NodePtr root);
    
private:
  OptimizationContext& context;
    
    bool canVectorize(NodePtr loopNode);
    NodePtr createVectorizedLoop(NodePtr loopNode);
    int getVectorWidth(); // Based on available SIMD instructions
};

// -----------------------------------------------------------------------------
// Look-Ahead Optimizer - Predictive optimization based on code patterns
// -----------------------------------------------------------------------------
class LookAheadOptimizer {
public:
    NodePtr optimize(NodePtr root);
    
private:
    // Predict future code paths and pre-optimize
    std::vector<NodePtr> predictLikelyPaths(NodePtr node);
    
    // Inline small frequently-called functions
 NodePtr inlineHotFunctions(NodePtr root);
    
    // Pre-compute likely branches
    NodePtr precomputeBranches(NodePtr node);
};

// -----------------------------------------------------------------------------
// Memory Optimization - Labeled containers and lock management
// -----------------------------------------------------------------------------
class MemoryOptimizer {
public:
    MemoryOptimizer(OptimizationContext& ctx) : context(ctx) {}
    
    NodePtr optimize(NodePtr root);
    
private:
    OptimizationContext& context;
    
    // Analyze lifetime of variables for better allocation
    struct VariableLifetime {
        std::string name;
        size_t firstUse;
        size_t lastUse;
        size_t size;
    };
    
    std::vector<VariableLifetime> analyzeLifetimes(NodePtr root);
    
    // Register allocation with spilling
    void optimizeRegisterAllocation(NodePtr root);
    
    // Memory pooling for same-sized allocations
    NodePtr createMemoryPools(NodePtr root);
    
// Lock coalescing for multi-threaded code
    NodePtr coalesceLocks(NodePtr root);
 
    // Cache-line alignment for hot data
    void alignHotData(NodePtr root);
};

// -----------------------------------------------------------------------------
// Resource Footprint Compression
// -----------------------------------------------------------------------------
class FootprintCompressor {
public:
    NodePtr compress(NodePtr root);
    
private:
    // Remove redundant operations
    NodePtr removeRedundancy(NodePtr node);
    
    // Merge similar code paths
    NodePtr mergePaths(NodePtr node);
    
    // Compress data structures
    NodePtr compressDataStructures(NodePtr node);
};

// -----------------------------------------------------------------------------
// Deductive Reasoning Engine - Learn from execution patterns
// -----------------------------------------------------------------------------
class DeductiveOptimizer {
public:
    DeductiveOptimizer(OptimizationContext& ctx) : context(ctx) {}
    
    // Analyze execution and deduce optimization opportunities
    void learn(NodePtr root, const std::vector<uint64_t>& executionTrace);
    
    // Apply learned optimizations
    NodePtr optimize(NodePtr root);
    
private:
    OptimizationContext& context;
    
    struct LearnedPattern {
   std::string pattern;
        std::function<NodePtr(NodePtr)> optimization;
        double confidence;
    };
    
    std::vector<LearnedPattern> learnedPatterns;
    
    // Detect patterns in execution
    void detectPatterns(const std::vector<uint64_t>& trace);
    
    // Generate derivative optimizations
    NodePtr generateDerivativeOptimization(NodePtr node);
};

// -----------------------------------------------------------------------------
// Optimization Chain - Applies multiple optimizations in sequence
// -----------------------------------------------------------------------------
class OptimizationChain {
public:
    OptimizationChain(OptimizationContext& ctx) : context(ctx) {}
    
    NodePtr optimize(NodePtr root);
    
    // Configure optimization levels
    void setAggressiveness(int level); // 0-3: O0, O1, O2, O3
    void enableLTO() { ltoEnabled = true; }
    void enablePGO() { pgoEnabled = true; }
    
private:
    OptimizationContext& context;
    int aggressiveness = 2;
    bool ltoEnabled = false;
    bool pgoEnabled = false;
    
    // Multi-pass optimization
    NodePtr runPasses(NodePtr root);
    
    // Pass ordering based on dependencies
    std::vector<std::function<NodePtr(NodePtr)>> getPasses();
};

// -----------------------------------------------------------------------------
// Curling Optimizer - Aggressive loop fusion and data locality
// -----------------------------------------------------------------------------
class CurlingOptimizer {
public:
    NodePtr optimize(NodePtr root);
    
private:
    // Fuse adjacent loops operating on same data
    NodePtr fuseLoops(NodePtr node);
    
    // Tiling for cache optimization
    NodePtr tileLoops(NodePtr node);
    
    // Data layout transformation for better locality
    NodePtr optimizeDataLayout(NodePtr node);
};

// -----------------------------------------------------------------------------
// Synchronized Scheduler - Multi-threaded optimization coordination
// -----------------------------------------------------------------------------
class SynchronizedScheduler {
public:
    SynchronizedScheduler(OptimizationContext& ctx) : context(ctx) {}
    
    NodePtr schedule(NodePtr root);

private:
    OptimizationContext& context;
    
    // Identify parallelizable sections
    std::vector<NodePtr> findParallelRegions(NodePtr root);
    
    // Schedule tasks across cores
    NodePtr scheduleTasks(const std::vector<NodePtr>& tasks);
    
    // Minimize synchronization overhead
    NodePtr optimizeSynchronization(NodePtr node);
};

// -----------------------------------------------------------------------------
// Master Optimization Engine
// -----------------------------------------------------------------------------
class MasterOptimizer {
public:
    MasterOptimizer();
    
    // Primary optimization entry point
    NodePtr optimize(NodePtr root, int optimizationLevel = 2);
    
    // Enable profile-guided optimization
    void enablePGO(const std::string& profileData);
    
    // Get optimization context for inspection
    const OptimizationContext& getContext() const { return context; }
    
private:
    OptimizationContext context;
    std::unique_ptr<OptimizationChain> chain;
    
void detectEnvironment();
    void loadProfileData(const std::string& profileData);
};

} // namespace Optimization

#endif // OPTIMIZATION_ENGINE_HPP
