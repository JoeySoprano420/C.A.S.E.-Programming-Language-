#pragma once
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
#include <random>
#include <atomic>
#include <condition_variable>
#include <map>
#include <cmath>
#include <queue>

// C++14 compatible clamp function
namespace AstroLake {
    template<typename T>
    inline T clamp(T value, T min_val, T max_val) {
        return (value < min_val) ? min_val : ((value > max_val) ? max_val : value);
    }
}

namespace AstroLake {

enum class CoreType { E_CORE, P_CORE };

struct ThreadDescriptor {
    int id;
    CoreType coreType;
    int zOffset;
    double temperature;
    double energy;
    double performance;
    double perfPerWatt;
    int socketID;
    int x; // Grid X position
};

struct Message {
    int srcSocket;
    int dstSocket;
    std::string payload;
};

class MessageBus {
    std::map<int, std::queue<Message>> inbox;
    std::mutex mu;
public:
    void send(int from, int to, const std::string& data) {
        std::lock_guard<std::mutex> l(mu);
        inbox[to].push({from, to, data});
    }
    
    bool recv(int sid, Message& out) {
        std::lock_guard<std::mutex> l(mu);
        if (inbox[sid].empty()) return false;
        out = inbox[sid].front();
        inbox[sid].pop();
        return true;
    }
};

struct AIBalancer {
    double wTemp = 0.05;
    double wPerf = 0.9;
    double bias = 0.1;
    double memory = 0.0;
    
    double predict(double temp, double perf) {
        double x = wPerf * perf - wTemp * (temp - 30.0) + bias + memory * 0.1;
        memory = std::tanh(x);
        return AstroLake::clamp(1.0 + memory, 0.3, 1.8);
    }
    
    void learn(double target) {
        double grad = target - memory;
        wPerf += 0.001 * grad;
        wTemp -= 0.001 * grad;
        bias += 0.0005 * grad;
    }
};

struct ThermalGrid {
    std::vector<std::vector<double>> grid;
    
    explicit ThermalGrid(int z) {
        grid.assign(3, std::vector<double>(z, 30.0));
    }
    
    void addHeat(int x, int z, double h) {
        if (x >= 0 && x < 3 && z >= 0 && z < static_cast<int>(grid[0].size())) {
            grid[x][z] += h;
        }
    }
    
    void diffuse() {
        auto g2 = grid;
        for (int x = 0; x < 3; x++) {
            for (size_t z = 0; z < grid[0].size(); z++) {
                double sum = 0.0;
                int c = 0;
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        int nx = x + dx;
                        int nz = static_cast<int>(z) + dz;
                        if (nx >= 0 && nx < 3 && nz >= 0 && nz < static_cast<int>(grid[0].size())) {
                            sum += grid[nx][nz];
                            c++;
                        }
                    }
                }
                if (c > 0) {
                    g2[x][z] = 0.9 * grid[x][z] + 0.1 * (sum / c);
                }
            }
        }
        grid.swap(g2);
    }
    
    double get(int x, int z) const {
        if (x >= 0 && x < 3 && z >= 0 && z < static_cast<int>(grid[0].size())) {
            return grid[x][z];
        }
        return 30.0;
    }
};

class AstroLakeCore {
public:
    AstroLakeCore(CoreType t, int id, int z)
        : type(t), coreID(id), zOffset(z), temperature(40.0), energy(0.0) {}

    double execute(ThreadDescriptor& td, AIBalancer& ai) {
        double workIntensity = (type == CoreType::P_CORE) ? 1.6 : 0.6;
        int timeMs = (type == CoreType::P_CORE) ? 35 : 80;

        td.performance = workIntensity * (100.0 - td.temperature) / 100.0;
        td.energy += workIntensity * 0.1;
        td.temperature += workIntensity * 0.9;
        
        double scale = ai.predict(td.temperature, td.performance);
        td.perfPerWatt = scale;
        ai.learn(td.perfPerWatt);
        
        dissipateHeat();
        std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
        
        return scale;
    }

    void dissipateHeat() {
        temperature -= 0.3;
        if (temperature < 25.0) temperature = 25.0;
    }

    CoreType getType() const { return type; }
    double getTemp() const { return temperature; }

private:
    CoreType type;
    int coreID;
    int zOffset;
    double temperature;
    double energy;
};

class AstroSocket {
public:
    AstroSocket(int sid, int eCount, int pCount, int z)
        : socketID(sid), zLayers(z), grid(z) {
        int id = 0;
        for (int i = 0; i < eCount; ++i)
            eCores.emplace_back(CoreType::E_CORE, id++, rand() % z);
        for (int i = 0; i < pCount; ++i)
            pCores.emplace_back(CoreType::P_CORE, id++, rand() % z);
    }

    AstroLakeCore* pickCore(CoreType t) {
        auto& pool = (t == CoreType::P_CORE) ? pCores : eCores;
        if (pool.empty()) return nullptr;
        return &pool[rand() % pool.size()];
    }

    int socketID;
    int zLayers;
    std::vector<AstroLakeCore> eCores, pCores;
    ThermalGrid grid;
};

class StarCluster {
public:
    StarCluster(int sockets, int ePer, int pPer, int z);
    void submit(ThreadDescriptor td);
    void run();
    void stop();

private:
    void workerLoop();
    void dashboard();

    std::vector<AstroSocket> clusters;
    MessageBus bus;
    AIBalancer ai;
    std::vector<std::thread> workers;
    std::thread dashboardThread;
    std::vector<ThreadDescriptor> queue;
    std::mutex queueMu;
    std::condition_variable cv;
    std::atomic<bool> stopFlag;
};

// Utility function
void runAstroLakeSimulation();

} // namespace AstroLake
