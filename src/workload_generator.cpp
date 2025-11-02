#include "hso_engine.h"

#include <algorithm>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::high_resolution_clock;
using namespace std::chrono_literals;

struct LatencyRecorder {
    std::mutex mu;
    std::vector<double> latMicros;
    void add(double us) {
        std::lock_guard<std::mutex> lg(mu);
        latMicros.push_back(us);
    }
    double percentile(double p) {
        std::lock_guard<std::mutex> lg(mu);
        if (latMicros.empty()) return 0.0;
        std::vector<double> v = latMicros;
        std::sort(v.begin(), v.end());
        size_t idx = static_cast<size_t>(std::ceil(p * (v.size() - 1)));
        if (idx >= v.size()) idx = v.size() - 1;
        return v[idx];
    }
};

int main() {
    HSO_Engine engine;

    // Load config
    std::string err;
    if (!engine.loadPoliciesFromFile("config.yaml", err)) {
        std::cerr << "Config load failed: " << err << "\n";
        return 1;
    }

    // Convenience references
    auto mram = engine.getDevice("MRAM0");
    auto qlcA = engine.getDevice("QLC_A");
    auto qlcB = engine.getDevice("QLC_B");
    if (!mram || !qlcA) {
        std::cerr << "Devices missing; check config.yaml\n";
        return 1;
    }

    LatencyRecorder criticalLat;

    std::atomic<bool> stop{false};
    auto deadline = Clock::now() + 10s;

    // Critical Thread: small random writes via engine tag CRITICAL_METADATA
    std::thread tCritical([&] {
        while (Clock::now() < deadline) {
            IORequest req{"CRITICAL_METADATA", 4 * 1024, IOType::RandomWrite};
            auto t0 = Clock::now();
            engine.submit(req);
            auto t1 = Clock::now();
            double us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            criticalLat.add(us);
        }
    });

    // Sequential Thread: large sequential writes via engine tag SENSOR_LOGS to QLC with ZNS mode
    std::thread tSequential([&] {
        while (Clock::now() < deadline) {
            IORequest req{"SENSOR_LOGS", 128 * 1024, IOType::SequentialWrite};
            engine.submit(req);
        }
    });

    // Baseline Thread: small random writes directly to QLC device without ZNS policy
    std::thread tBaseline([&] {
        if (!qlcB) return; // if only one QLC, baseline will share it
        while (Clock::now() < deadline) {
            IORequest req{"BASELINE_CRITICAL", 4 * 1024, IOType::RandomWrite};
            qlcB->submitWrite(req);
        }
    });

    // Optional: Simulated ARCHIVE_DATA dynamic routing for Phase 2 scaffolding
    std::thread tArchive([&] {
        while (Clock::now() < deadline) {
            IORequest req{"ARCHIVE_DATA", 256 * 1024, IOType::SequentialWrite};
            engine.submitArchiveDynamic(req);
        }
    });

    tCritical.join();
    tSequential.join();
    tBaseline.join();
    tArchive.join();

    double p999 = criticalLat.percentile(0.999);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "CRITICAL 99.9th percentile latency (us): " << p999 << "\n";

    auto wearSeqDevice = qlcA ? qlcA : qlcB;
    std::uint64_t wearSeq = wearSeqDevice ? wearSeqDevice->wearLevel() : 0;
    std::uint64_t wearBaseline = qlcB ? qlcB->wearLevel() : 0;
    std::cout << "Wear (Sequential ZNS device): " << wearSeq << "\n";
    std::cout << "Wear (Baseline random device): " << wearBaseline << "\n";

    return 0;
}


