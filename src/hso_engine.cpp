#include "hso_engine.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {

static std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e-1]))) --e;
    return s.substr(b, e - b);
}

}

void HSO_Engine::registerDevice(std::shared_ptr<StorageDevice> dev) {
    devices_[dev->name()] = std::move(dev);
}

std::shared_ptr<StorageDevice> HSO_Engine::getDevice(const std::string& name) const {
    auto it = devices_.find(name);
    if (it != devices_.end()) return it->second;
    return nullptr;
}

bool HSO_Engine::loadPoliciesFromFile(const std::string& path, std::string& errMsg) {
    // Minimalistic parser for expected config.yaml structure for the demo
    // Expected sections:
    // devices: list with name, type, capacity
    // policies: list with data_tag, target, mode
    std::ifstream in(path);
    if (!in.is_open()) {
        errMsg = "Failed to open config file";
        return false;
    }
    std::string line;
    enum class Section { None, Devices, Policies } section = Section::None;
    std::string curName, curType, curMode, curTarget, curTag;
    std::uint64_t curCapacity = 0;

    auto flushDevice = [&]() {
        if (curName.empty() || curType.empty()) return;
        if (devices_.count(curName) != 0) return;
        if (curType == "MRAM_Device") {
            registerDevice(std::make_shared<MRAM_Device>(curName, curCapacity));
        } else if (curType == "QLC_NAND_Device") {
            registerDevice(std::make_shared<QLC_NAND_Device>(curName, curCapacity));
        }
        curName.clear(); curType.clear(); curCapacity = 0;
    };

    auto flushPolicy = [&]() {
        if (curTag.empty() || curTarget.empty()) return;
        PolicyRule r{curTag, curTarget, curMode};
        rulesByTag_[curTag] = r;
        curTag.clear(); curTarget.clear(); curMode.clear();
    };

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line == "devices:") { section = Section::Devices; continue; }
        if (line == "policies:") { section = Section::Policies; continue; }

        if (section == Section::Devices) {
            if (line == "-") continue;
            if (line.rfind("- ", 0) == 0) { // new item begins; flush previous if any
                flushDevice();
                line = trim(line.substr(2));
            }
            if (line.rfind("name:", 0) == 0) {
                curName = trim(line.substr(5));
            } else if (line.rfind("type:", 0) == 0) {
                curType = trim(line.substr(5));
            } else if (line.rfind("capacity:", 0) == 0) {
                curCapacity = std::stoull(trim(line.substr(9)));
            }
        } else if (section == Section::Policies) {
            if (line == "-") continue;
            if (line.rfind("- ", 0) == 0) { // new policy
                flushPolicy();
                line = trim(line.substr(2));
            }
            if (line.rfind("data_tag:", 0) == 0) {
                curTag = trim(line.substr(9));
            } else if (line.rfind("target:", 0) == 0) {
                curTarget = trim(line.substr(7));
            } else if (line.rfind("mode:", 0) == 0) {
                curMode = trim(line.substr(5));
            }
        }
    }
    flushDevice();
    flushPolicy();
    return true;
}

void HSO_Engine::submit(const IORequest& req) {
    auto it = rulesByTag_.find(req.dataTag);
    if (it == rulesByTag_.end()) {
        // No rule: drop silently in demo
        return;
    }
    const PolicyRule& r = it->second;
    auto dev = getDevice(r.targetDeviceName);
    if (!dev) return;
    IORequest mutableReq = req;
    if (r.mode == "ZNS_SEQ_WRITE") {
        mutableReq.type = IOType::SequentialWrite;
    } else {
        mutableReq.type = IOType::RandomWrite;
    }
    dev->submitWrite(mutableReq);
}

void HSO_Engine::submitArchiveDynamic(const IORequest& req) {
    // Choose among QLC devices the least worn
    std::shared_ptr<StorageDevice> best;
    for (const auto& [name, dev] : devices_) {
        if (name.find("QLC_") == 0 || name.find("QLC") == 0) {
            if (!best || dev->wearLevel() < best->wearLevel()) {
                best = dev;
            }
        }
    }
    if (!best) return;
    IORequest seqReq = req;
    seqReq.type = IOType::SequentialWrite; // prefer sequential for archive
    best->submitWrite(seqReq);
}


