#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>

enum class IOType {
    RandomWrite,
    SequentialWrite
};

struct IORequest {
    std::string dataTag; // e.g., CRITICAL_METADATA, SENSOR_LOGS, ARCHIVE_DATA
    std::size_t sizeBytes;
    IOType type;
};

class StorageDevice {
public:
    StorageDevice(std::string name, std::uint64_t capacityBytes)
        : name_(std::move(name)), capacityBytes_(capacityBytes), usedBytes_(0), pendingIOs_(0), wearLevelFactor_(0) {}
    virtual ~StorageDevice() = default;

    const std::string& name() const { return name_; }

    virtual void submitWrite(const IORequest& req) = 0;

    std::uint64_t capacityBytes() const { return capacityBytes_; }
    std::uint64_t usedBytes() const { return usedBytes_.load(); }

    // Phase 2: queue depth exposure
    std::uint64_t get_current_queue_depth() const { return pendingIOs_.load(); }

    // Wear level factor (QLC uses it; MRAM keeps 0)
    std::uint64_t wearLevel() const { return wearLevelFactor_.load(); }

protected:
    void accountAllocation(std::size_t bytes) { usedBytes_.fetch_add(bytes, std::memory_order_relaxed); }
    void incPending() { pendingIOs_.fetch_add(1, std::memory_order_relaxed); }
    void decPending() { pendingIOs_.fetch_sub(1, std::memory_order_relaxed); }
    void addWear(std::uint64_t v) { wearLevelFactor_.fetch_add(v, std::memory_order_relaxed); }

private:
    std::string name_;
    std::uint64_t capacityBytes_;
    std::atomic<std::uint64_t> usedBytes_;
    std::atomic<std::uint64_t> pendingIOs_;
    std::atomic<std::uint64_t> wearLevelFactor_;
};

class MRAM_Device : public StorageDevice {
public:
    explicit MRAM_Device(std::string name, std::uint64_t capacityBytes)
        : StorageDevice(std::move(name), capacityBytes) {}

    void submitWrite(const IORequest& req) override;
};

class QLC_NAND_Device : public StorageDevice {
public:
    explicit QLC_NAND_Device(std::string name, std::uint64_t capacityBytes)
        : StorageDevice(std::move(name), capacityBytes), rng_(std::random_device{}()) {}

    // mode: random or zns sequential influences latency and wear
    void submitWrite(const IORequest& req) override;

private:
    std::mt19937 rng_;
};


