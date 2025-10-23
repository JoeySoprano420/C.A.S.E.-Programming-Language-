//=============================================================================
//  Violet Aura Creations — AstroLake Hardware Simulation Cluster
//  Simulates Intel AstroLake architecture with E/P cores, photonic bus,
//  LSTM thermal prediction, and profiling feedback
//=============================================================================

#pragma once

#include "HexIR.hpp"
#include "MultiTierOptimizer.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <queue>

namespace AstroLake {

// =============================================================================
// Core Types
// =============================================================================

enum class CoreType {
    EFFICIENCY,  // E-cores (lower power, simpler pipeline)
    PERFORMANCE  // P-cores (higher power, complex OoO pipeline)
};

// =============================================================================
// Socket/Cluster Architecture
// =============================================================================

struct Socket {
    int socketId;
    std::vector<int> pCoreIds;
    std::vector<int> eCoreIds;
    size_t l3CacheSize;  // Shared L3 cache
    
    Socket(int id) 
: socketId(id), l3CacheSize(32 * 1024 * 1024) {}  // 32MB default
};

struct StarCluster {
    std::string name;
    std::vector<Socket> sockets;
    size_t photonicBusBandwidth;  // GB/s
    
    StarCluster(const std::string& name, int numSockets)
        : name(name), photonicBusBandwidth(1000) {  // 1 TB/s photonic
  for (int i = 0; i < numSockets; ++i) {
            sockets.push_back(Socket(i));
        }
    }
};

// =============================================================================
// Core Simulator
// =============================================================================

class CoreSimulator {
public:
    struct CoreConfig {
        CoreType type;
        int coreId;
 double baseFrequency;  // GHz
        double boostFrequency;
        int pipelineWidth;  // Instructions per cycle
 int reorderBufferSize;
  size_t l1ICacheSize;
   size_t l1DCacheSize;
        size_t l2CacheSize;
        
        CoreConfig(CoreType t, int id)
         : type(t), coreId(id) {
 if (t == CoreType::PERFORMANCE) {
     baseFrequency = 3.2;
boostFrequency = 5.8;
       pipelineWidth = 6;
 reorderBufferSize = 512;
 l1ICacheSize = 64 * 1024;
 l1DCacheSize = 64 * 1024;
       l2CacheSize = 2 * 1024 * 1024;
        } else {
baseFrequency = 2.0;
         boostFrequency = 3.8;
   pipelineWidth = 4;
reorderBufferSize = 256;
                l1ICacheSize = 32 * 1024;
         l1DCacheSize = 32 * 1024;
            l2CacheSize = 1 * 1024 * 1024;
   }
    }
    };
    
    CoreSimulator(const CoreConfig& config)
        : config(config)
        , currentFrequency(config.baseFrequency)
        , temperature(40.0)
        , powerUsage(0.0)
        , cyclesExecuted(0)
        , instructionsRetired(0)
    {}
    
    // Execute instruction
    void executeInstruction(HexIR::InstructionPtr inst);
    
    // Get current status
    double getTemperature() const { return temperature; }
    double getPowerUsage() const { return powerUsage; }
    double getIPC() const {
     return cyclesExecuted > 0 ? 
            (double)instructionsRetired / cyclesExecuted : 0.0;
 }
    
  // Update thermal state
    void updateThermals(double ambientTemp, double coolingEfficiency);

    // Adjust frequency based on thermal/power
    void adjustFrequency();
    
private:
    CoreConfig config;
    double currentFrequency;
    double temperature;  // Celsius
    double powerUsage;   // Watts
    uint64_t cyclesExecuted;
    uint64_t instructionsRetired;
    
    // Instruction latencies (in cycles)
    int getInstructionLatency(HexIR::OpCode opcode) const;
    double calculatePower(HexIR::OpCode opcode) const;
};

// =============================================================================
// LSTM Thermal Predictor
// =============================================================================

class LSTMThermalPredictor {
public:
    struct LSTMState {
        std::vector<double> cellState;
        std::vector<double> hiddenState;
        
 LSTMState(int size) 
            : cellState(size, 0.0)
       , hiddenState(size, 0.0) {}
    };
    
    LSTMThermalPredictor(int hiddenSize = 64)
        : hiddenSize(hiddenSize)
        , state(hiddenSize) {
        initializeWeights();
}
    
    // Predict temperature N steps ahead
 double predictTemperature(const std::vector<double>& history, int stepsAhead);
    
    // Update model with actual measurement
    void update(double actual, double predicted);
    
    // Get prediction confidence
    double getConfidence() const { return confidence; }
    
private:
    int hiddenSize;
    LSTMState state;
    double confidence;
    
    // LSTM parameters (simplified)
    std::vector<std::vector<double>> weightsInput;
    std::vector<std::vector<double>> weightsHidden;
    std::vector<double> bias;
    
    void initializeWeights();
    double sigmoid(double x) const;
  double tanh(double x) const;
    std::vector<double> forward(const std::vector<double>& input);
};

// =============================================================================
// Photonic Bus Simulator
// =============================================================================

class PhotonicBus {
public:
    struct Transfer {
        int sourceSocket;
        int destSocket;
        size_t dataSize;
  uint64_t startCycle;
        uint64_t endCycle;
    };
    
    PhotonicBus(size_t bandwidthGBps)
    : bandwidthGBps(bandwidthGBps)
        , currentCycle(0)
        , totalBytesTransferred(0) {}
    
    // Schedule transfer
    bool scheduleTransfer(int src, int dest, size_t size);
    
    // Advance simulation
    void tick();
    
    // Get utilization
    double getUtilization() const;
    
    // Get latency for transfer
    uint64_t getLatency(size_t dataSize) const;
    
private:
    size_t bandwidthGBps;
    uint64_t currentCycle;
    size_t totalBytesTransferred;
std::queue<Transfer> pendingTransfers;
    std::vector<Transfer> activeTransfers;
};

// =============================================================================
// Cache & Branch Profiling Sensors
// =============================================================================

class CacheProfilingSensor {
public:
struct CacheEvent {
        std::string location;
        bool hit;
 uint64_t address;
   uint64_t cycle;
    };
    
    CacheProfilingSensor()
        : totalAccesses(0)
        , totalHits(0)
        , totalMisses(0) {}
    
    void recordAccess(const std::string& location, bool hit, uint64_t address);
    
  double getHitRate() const {
        return totalAccesses > 0 ? (double)totalHits / totalAccesses : 0.0;
    }
    
    double getMissRate() const { return 1.0 - getHitRate(); }
    
    std::unordered_map<std::string, double> getLocationHitRates() const;
    
private:
    uint64_t totalAccesses;
    uint64_t totalHits;
    uint64_t totalMisses;
    std::vector<CacheEvent> events;
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> locationStats;
};

class BranchProfilingSensor {
public:
    struct BranchEvent {
      std::string location;
        bool taken;
        bool predicted;
  bool correct;
        uint64_t cycle;
    };
    
    BranchProfilingSensor()
        : totalBranches(0)
      , totalCorrect(0)
  , totalMispredicted(0) {}
    
    void recordBranch(const std::string& location, bool taken, bool predicted);
    
    double getAccuracy() const {
        return totalBranches > 0 ? (double)totalCorrect / totalBranches : 0.0;
    }
    
    double getMispredictionRate() const { return 1.0 - getAccuracy(); }
    
    std::unordered_map<std::string, double> getLocationAccuracies() const;
    
private:
uint64_t totalBranches;
    uint64_t totalCorrect;
    uint64_t totalMispredicted;
 std::vector<BranchEvent> events;
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> locationStats;
};

// =============================================================================
// AstroLake Cluster Simulator
// =============================================================================

class ClusterSimulator {
public:
    struct SimulationConfig {
        int numSockets;
        int pCoresPerSocket;
    int eCoresPerSocket;
        double ambientTemperature;
        double coolingEfficiency;
 bool enableThermalPrediction;
        bool enableCacheProfiling;
bool enableBranchProfiling;
        bool enablePhotonicBus;
        
        SimulationConfig()
            : numSockets(2)
 , pCoresPerSocket(8)
     , eCoresPerSocket(16)
 , ambientTemperature(25.0)
         , coolingEfficiency(0.9)
    , enableThermalPrediction(true)
            , enableCacheProfiling(true)
    , enableBranchProfiling(true)
       , enablePhotonicBus(true)
        {}
    };
    
    ClusterSimulator(const SimulationConfig& config);
    
    // Execute IR module on cluster
    void execute(HexIR::ModulePtr module);
  
    // Get feedback data
    Optimization::AutoFDOCounters getFeedbackCounters() const;
    
    // Get profiling data
  const CacheProfilingSensor& getCacheSensor() const { return cacheSensor; }
    const BranchProfilingSensor& getBranchSensor() const { return branchSensor; }
    
    // Get thermal predictions
    std::vector<double> predictTemperatures(int stepsAhead) const;

    // Print simulation statistics
    void printStatistics() const;
  
private:
    SimulationConfig config;
    StarCluster cluster;
    std::vector<std::unique_ptr<CoreSimulator>> cores;
    std::unique_ptr<PhotonicBus> photonicBus;
    LSTMThermalPredictor thermalPredictor;
    CacheProfilingSensor cacheSensor;
    BranchProfilingSensor branchSensor;
    Optimization::AutoFDOCounters feedbackCounters;
    
    std::vector<double> temperatureHistory;
    uint64_t currentCycle;
    
    // Schedule instruction to core
    int scheduleInstruction(HexIR::InstructionPtr inst);
  
    // Update sensors
    void updateSensors(HexIR::InstructionPtr inst, int coreId);
};

// =============================================================================
// Adaptive Re-optimization Engine
// =============================================================================

class AdaptiveReoptimizer {
public:
    AdaptiveReoptimizer()
        : reoptimizationThreshold(1000)
   , significantChangeThreshold(0.1) {}
    
    // Check if reoptimization is needed
    bool shouldReoptimize(const Optimization::AutoFDOCounters& currentCounters,
const Optimization::AutoFDOCounters& previousCounters) const;
    
 // Perform runtime reoptimization
    void reoptimize(HexIR::ModulePtr module,
     const Optimization::AutoFDOCounters& counters,
            Optimization::OptimizationPipeline& pipeline);
    
    // Update introspection database
    void updateDatabase(const std::string& function,
 const Optimization::ProfileDataManager::ProfileEntry& entry);
    
 // Get learned patterns
    std::vector<std::string> getLearnedPatterns() const;
    
private:
    uint64_t reoptimizationThreshold;
    double significantChangeThreshold;
    
    struct IntrospectionEntry {
        std::string function;
        std::vector<Optimization::ProfileDataManager::ProfileEntry> history;
    std::unordered_map<std::string, double> learnedPatterns;
    };
    
    std::unordered_map<std::string, IntrospectionEntry> database;
  
    bool isSignificantChange(const Optimization::AutoFDOCounters& current,
               const Optimization::AutoFDOCounters& previous) const;
};

} // namespace AstroLake
