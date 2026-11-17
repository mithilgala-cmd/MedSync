#pragma once

#include <cstdint>
#include <string>

#include "medsync/Types.hpp"

namespace medsync {

// Events published by worker threads and consumed by the controller's
// supervisor thread. Workers never change device state directly.
enum class DeviceEventType {
    MoveCompleted,
    MoveAborted,
    MoveFailed,
    ScanCompleted,
    ScanFailed,
};

struct DeviceEvent {
    DeviceEventType type{DeviceEventType::MoveCompleted};
    // Correlates a worker result with the cycle that requested it, so a stale
    // event can never complete a later cycle.
    std::uint64_t cycleId{0};
    ScanResult scan{};
    std::string message;
};

} // namespace medsync
