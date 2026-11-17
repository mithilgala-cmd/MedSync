#include "medsync/PositioningController.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "medsync/Logger.hpp"

namespace medsync {

PositioningController::PositioningController(const Config& config,
                                             Logger& logger)
    : config_(config), logger_(logger) {}

bool PositioningController::home(std::string& /*error*/) {
    position_ = config_.homePosition;
    homed_ = true;
    logger_.info("stage homed at (" + std::to_string(position_.x) + ", " +
                 std::to_string(position_.y) + ")");
    return true;
}

bool PositioningController::moveTo(const Position& target, std::string& error) {
    if (!homed_) {
        error = "stage must be homed before moving";
        return false;
    }
    if (!config_.contains(target)) {
        error = "target (" + std::to_string(target.x) + ", " +
                std::to_string(target.y) + ") is outside the stage bounds";
        return false;
    }

    logger_.debug("move requested to (" + std::to_string(target.x) + ", " +
                  std::to_string(target.y) + ")");

    moveAxis(position_.x, target.x, 'x');
    moveAxis(position_.y, target.y, 'y');

    logger_.info("stage reached (" + std::to_string(position_.x) + ", " +
                 std::to_string(position_.y) + ")");
    return true;
}

const Position& PositioningController::position() const noexcept {
    return position_;
}

bool PositioningController::isHomed() const noexcept {
    return homed_;
}

void PositioningController::invalidateHoming() noexcept {
    homed_ = false;
}

void PositioningController::moveAxis(int& current, int target, char axisName) {
    // Complexity: O(|target - current| / stepSize) iterations. Each iteration
    // is constant work, so the loop is linear in the number of steps taken.
    while (current != target) {
        const int remaining = target - current;
        const int step = std::min(config_.stepSize, std::abs(remaining));
        current += (remaining > 0) ? step : -step;
        logger_.debug(std::string("axis ") + axisName + " -> " +
                      std::to_string(current));
    }
}

} // namespace medsync
