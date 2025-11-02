#pragma once

#include "hso_device.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

struct PolicyRule {
    std::string dataTag;          // e.g., CRITICAL_METADATA
    std::string targetDeviceName; // e.g., MRAM0, QLC_A
    std::string mode;             // e.g., RANDOM, ZNS_SEQ_WRITE
};

class HSO_Engine {
public:
    void registerDevice(std::shared_ptr<StorageDevice> dev);

    // Load static policies from a YAML-like file (simple parser for MVD)
    bool loadPoliciesFromFile(const std::string& path, std::string& errMsg);

    // Route according to policies. Returns latency in microseconds measured externally.
    void submit(const IORequest& req);

    // Phase 2: dynamic routing for ARCHIVE_DATA with wear-aware selection among QLCs
    void submitArchiveDynamic(const IORequest& req);

    std::shared_ptr<StorageDevice> getDevice(const std::string& name) const;

    const std::unordered_map<std::string, std::shared_ptr<StorageDevice>>& devices() const { return devices_; }

private:
    std::unordered_map<std::string, std::shared_ptr<StorageDevice>> devices_;
    std::unordered_map<std::string, PolicyRule> rulesByTag_;
};


