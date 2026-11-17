#include "medsync/DeviceState.hpp"

namespace medsync {

std::string_view to_string(DeviceState state) noexcept {
    switch (state) {
    case DeviceState::Idle:
        return "Idle";
    case DeviceState::Homing:
        return "Homing";
    case DeviceState::Ready:
        return "Ready";
    case DeviceState::Positioning:
        return "Positioning";
    case DeviceState::Scanning:
        return "Scanning";
    case DeviceState::Fault:
        return "Fault";
    case DeviceState::EmergencyStop:
        return "EmergencyStop";
    case DeviceState::ShuttingDown:
        return "ShuttingDown";
    }
    return "Unknown";
}

bool canTransition(DeviceState from, DeviceState to) noexcept {
    if (from == to) {
        return false;
    }

    switch (from) {
    case DeviceState::Idle:
        return to == DeviceState::Homing ||
               to == DeviceState::EmergencyStop ||
               to == DeviceState::ShuttingDown;

    case DeviceState::Homing:
        return to == DeviceState::Ready ||
               to == DeviceState::Fault ||
               to == DeviceState::EmergencyStop ||
               to == DeviceState::ShuttingDown;

    case DeviceState::Ready:
        return to == DeviceState::Positioning ||
               to == DeviceState::Fault ||
               to == DeviceState::EmergencyStop ||
               to == DeviceState::ShuttingDown;

    case DeviceState::Positioning:
        return to == DeviceState::Scanning ||
               to == DeviceState::Fault ||
               to == DeviceState::EmergencyStop ||
               to == DeviceState::ShuttingDown;

    case DeviceState::Scanning:
        return to == DeviceState::Ready ||
               to == DeviceState::Fault ||
               to == DeviceState::EmergencyStop ||
               to == DeviceState::ShuttingDown;

    case DeviceState::Fault:
        return to == DeviceState::Idle ||
               to == DeviceState::EmergencyStop ||
               to == DeviceState::ShuttingDown;

    case DeviceState::EmergencyStop:
        return to == DeviceState::Idle ||
               to == DeviceState::ShuttingDown;

    case DeviceState::ShuttingDown:
        return false;
    }

    return false;
}

} // namespace medsync
