#include "hso_device.h"

#include <thread>

using namespace std::chrono_literals;

void MRAM_Device::submitWrite(const IORequest& req) {
    (void)req; // unused specifics for MRAM in this MVD
    incPending();
    // ~5us latency
    std::this_thread::sleep_for(std::chrono::microseconds(5));
    accountAllocation(req.sizeBytes);
    decPending();
}

void QLC_NAND_Device::submitWrite(const IORequest& req) {
    incPending();
    // simulate latency and wear based on io type
    if (req.type == IOType::SequentialWrite) {
        // ZNS-like sequential write: ~50us, low wear increment
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        addWear(1);
    } else {
        // Random write: ~100us, higher wear
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        addWear(10);
    }
    accountAllocation(req.sizeBytes);
    decPending();
}


