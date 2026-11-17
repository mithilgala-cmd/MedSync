#pragma once

#include <string_view>

namespace medsync {

// High-level state of the simulated device. Sample state is tracked
// separately by SampleHandler; do not conflate the two.
enum class DeviceState {
    Idle,
    Homing,
    Ready,
    Positioning,
    Scanning,
    Fault,
    EmergencyStop,
    ShuttingDown,
};

std::string_view to_string(DeviceState state) noexcept;

// True when the normal transition table allows `from -> to`.
// Emergency stop is allowed from every state except the terminal
// ShuttingDown state. Invalid transitions are rejected, never forced.
bool canTransition(DeviceState from, DeviceState to) noexcept;

} // namespace medsync
