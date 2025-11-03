#pragma once

#include "hso_device.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct PolicyRule {
	std::string tag;
	std::string targetDevice;
	std::string mode; // "RANDOM" or "ZNS_SEQ_WRITE"
};

class HSO_Engine {
public:
	HSO_Engine() = default;

	// Load a simple YAML-like config file
	bool loadConfig(const std::string &configPath, std::string &errorMessage);

	// Submit routed I/O based on tag using static policy (Phase 1)
	uint64_t submitRoutedIO(const IORequest &req);

	// Direct submit to a named device (baseline path)
	uint64_t submitDirect(const std::string &deviceName, const IORequest &req);

	// Phase 2: wear-aware dynamic routing for ARCHIVE_DATA across QLC devices
	uint64_t submitArchiveDynamic(const IORequest &req);

	// Accessors
	std::shared_ptr<StorageDevice> getDevice(const std::string &name) const;
	std::vector<std::shared_ptr<QLC_NAND_Device>> getQLCDevices() const;

private:
	bool parseConfig(const std::string &text, std::string &errorMessage);
	static std::string trim(const std::string &s);
	static bool starts_with(const std::string &s, const std::string &p);

private:
	std::unordered_map<std::string, std::shared_ptr<StorageDevice>> devices_;
	std::vector<PolicyRule> policies_;
};

