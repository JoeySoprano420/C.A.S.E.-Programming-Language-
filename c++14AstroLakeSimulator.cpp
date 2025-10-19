#include "AstroLakeSimulator.hpp"
#include <iostream>
#include <ctime>

// Conditional ncurses support
#ifdef _WIN32
    #define USE_NCURSES 0
#else
    #ifdef __has_include
        #if __has_include(<ncurses.h>)
            #include <ncurses.h>
            #define USE_NCURSES 1
        #else
            #define USE_NCURSES 0
        #endif
    #else
        #define USE_NCURSES 0
    #endif
#endif

namespace AstroLake {

StarCluster::StarCluster(int sockets, int ePer, int pPer, int z)
    : stopFlag(false) {
    for (int s = 0; s < sockets; ++s)
        clusters.emplace_back(s, ePer, pPer, z);
}

void StarCluster::submit(ThreadDescriptor td) {
    std::lock_guard<std::mutex> lock(queueMu);
    queue.push_back(td);
    cv.notify_one();
}

void StarCluster::run() {
#if USE_NCURSES
    initscr();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);
#else
    std::cout << "⭐ Starting Astro Lake Multi-Socket Simulation\n";
    std::cout << "(ncurses not available, using console output)\n\n";
#endif

    for (int i = 0; i < 12; ++i)
        workers.emplace_back(&StarCluster::workerLoop, this);

    dashboardThread = std::thread(&StarCluster::dashboard, this);
}

void StarCluster::stop() {
    stopFlag = true;
    cv.notify_all();
    
    for (auto& w : workers)
        if (w.joinable()) w.join();
    
    if (dashboardThread.joinable()) 
        dashboardThread.join();
    
#if USE_NCURSES
    endwin();
#endif
}

void StarCluster::workerLoop() {
    while (!stopFlag.load()) {
        ThreadDescriptor td;
        {
            std::unique_lock<std::mutex> lock(queueMu);
            cv.wait(lock, [this]() { 
                return stopFlag.load() || !queue.empty(); 
            });
            
            if (stopFlag.load()) break;
            
            if (queue.empty()) continue;
            
            td = queue.back();
            queue.pop_back();
        }

        AstroSocket& sock = clusters[td.socketID % clusters.size()];
        AstroLakeCore* core = sock.pickCore(td.coreType);

        if (!core) continue;
        
        double scale = core->execute(td, ai);

        // Update thermal grid
        sock.grid.addHeat(td.x, td.zOffset, td.temperature * 0.01);
        sock.grid.diffuse();

        // Thermal-aware warping
        if (td.temperature > 70.0) {
            int newSocket = (td.socketID + 1) % static_cast<int>(clusters.size());
            bus.send(td.socketID, newSocket, "warp");
            td.socketID = newSocket;
            td.zOffset = (td.zOffset + 1) % sock.zLayers;
        }

        // Performance-driven core type switching
        if (scale > 1.2)
            td.coreType = CoreType::E_CORE;
        else if (scale < 0.7)
            td.coreType = CoreType::P_CORE;

        // Re-submit for continuous processing
        submit(td);
    }
}

void StarCluster::dashboard() {
    int iteration = 0;
    
    while (!stopFlag.load()) {
#if USE_NCURSES
        erase();
        mvprintw(0, 0, "⭐ Multi-Socket Astro Lake Star Cluster");
        mvprintw(1, 0, "═══════════════════════════════════════");
        
        int line = 3;
        for (size_t s = 0; s < clusters.size(); ++s) {
            mvprintw(line++, 0, "Socket %zu:", s);
            mvprintw(line++, 2, "E-Cores: %zu | P-Cores: %zu",
                     clusters[s].eCores.size(), clusters[s].pCores.size());
            
            for (int z = 0; z < clusters[s].zLayers; ++z) {
                mvprintw(line, 4, "Z%d:", z);
                for (int x = 0; x < 3; ++x) {
                    double T = clusters[s].grid.get(x, z);
                    mvprintw(line, 15 + x * 10, "X%d: %.1f°C", x, T);
                }
                line++;
            }
            line++;
        }
        
        mvprintw(line++, 0, "═══════════════════════════════════════");
        mvprintw(line++, 0, "Iteration: %d | Press Ctrl+C to stop", iteration++);
        
        refresh();
#else
        // Console fallback with periodic updates
        if (iteration % 10 == 0) {
            std::cout << "\r⭐ Astro Lake Simulation - Iteration " << iteration 
                      << " | Sockets: " << clusters.size() 
                      << " | Running..." << std::flush;
        }
        iteration++;
#endif
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
#if !USE_NCURSES
    std::cout << "\n";
#endif
}

void runAstroLakeSimulation() {
    srand(static_cast<unsigned>(time(nullptr)));
    
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║   🌠 Astro Lake Multi-Socket Star Cluster Simulator   ║\n";
    std::cout << "║   3D Stacked E/P-Core Architecture with AI Balancing ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Configuration:\n";
    std::cout << "  - Sockets: 3\n";
    std::cout << "  - E-Cores per socket: 2\n";
    std::cout << "  - P-Cores per socket: 2\n";
    std::cout << "  - Z-layers: 3 (3D stacking)\n";
    std::cout << "  - Worker threads: 12\n";
    std::cout << "\n";
    
    StarCluster astroCluster(3, 2, 2, 3);
    astroCluster.run();

    // Submit initial thread descriptors
    std::cout << "Submitting 15 initial workloads...\n\n";
    for (int i = 0; i < 15; ++i) {
        ThreadDescriptor td = {
            i,                          // id
            CoreType::E_CORE,          // coreType
            i % 3,                      // zOffset
            30.0,                       // temperature
            0.0,                        // energy
            1.0,                        // performance
            1.0,                        // perfPerWatt
            i % 3,                      // socketID
            rand() % 3                  // x position
        };
        astroCluster.submit(td);
    }

    std::cout << "Simulation running for 12 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(12));
    
    astroCluster.stop();
    
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║ ✅ Multi-socket Astro Lake simulation complete!       ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

} // namespace AstroLake
