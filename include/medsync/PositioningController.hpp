#pragma once

#include <string>

#include "medsync/Config.hpp"
#include "medsync/Types.hpp"

namespace medsync {

class Logger;

// Simulated two-axis stage. Movement is a deterministic stepping loop: no
// threads and no sleeping in Milestone 1. Milestone 3 replaces the direct call
// with a worker thread that performs the same steps over simulated time.
class PositioningController {
public:
    PositioningController(const Config& config, Logger& logger);

    // Moves to the configured home position and marks the stage as homed.
    bool home(std::string& error);

    // Requires a homed stage and a target inside the configured bounds.
    bool moveTo(const Position& target, std::string& error);

    const Position& position() const noexcept;
    bool isHomed() const noexcept;

    // Called after an emergency stop: the stage must be homed again before it
    // can move. The physical position is not changed by this call.
    void invalidateHoming() noexcept;

private:
    void moveAxis(int& current, int target, char axisName);

    Config config_;
    Logger& logger_;
    Position position_{};
    bool homed_{false};
};

} // namespace medsync
