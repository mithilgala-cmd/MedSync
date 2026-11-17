#include "medsync/PositioningController.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

#include "medsync/Logger.hpp"

namespace medsync {

PositioningController::PositioningController(const Config& config,
                                             Logger& logger)
    : config_(config), logger_(logger) {}

PositioningController::~PositioningController() {
    shutdown();
}

void PositioningController::start(BoundedQueue<DeviceEvent>& events) {
    if (worker_.joinable()) {
        return;
    }
    events_ = &events;
    worker_ = std::thread(&PositioningController::run, this);
}

void PositioningController::shutdown() {
    abort_.store(true, std::memory_order_release);
    jobs_.close();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool PositioningController::waitUntilIdle(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(idleMutex_);
    return idleCv_.wait_for(lock, timeout, [this] { return !busy_; });
}

bool PositioningController::home(std::string& /*error*/) {
    Position homedAt;
    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        position_ = config_.homePosition;
        homed_ = true;
        homedAt = position_;
    }
    logger_.info("stage homed at (" + std::to_string(homedAt.x) + ", " +
                 std::to_string(homedAt.y) + ")");
    return true;
}

bool PositioningController::submitMove(const Position& target,
                                       std::uint64_t cycleId,
                                       std::string& error) {
    if (!worker_.joinable()) {
        error = "positioning worker is not running";
        return false;
    }

    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        if (!homed_) {
            error = "stage must be homed before moving";
            return false;
        }
        if (!config_.contains(target)) {
            error = "target (" + std::to_string(target.x) + ", " +
                    std::to_string(target.y) + ") is outside the stage bounds";
            return false;
        }
        generation = generation_.load(std::memory_order_acquire);
    }

    if (!jobs_.tryPush(MoveJob{target, cycleId, generation})) {
        error = "positioning queue is full or closed";
        return false;
    }
    return true;
}

bool PositioningController::moveTo(const Position& target, std::string& error) {
    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        if (!homed_) {
            error = "stage must be homed before moving";
            return false;
        }
    }
    if (!config_.contains(target)) {
        error = "target (" + std::to_string(target.x) + ", " +
                std::to_string(target.y) + ") is outside the stage bounds";
        return false;
    }

    const std::uint64_t generation =
        generation_.load(std::memory_order_acquire);

    logger_.debug("move requested to (" + std::to_string(target.x) + ", " +
                  std::to_string(target.y) + ")");

    if (!moveAxis(target.x, 'x', generation, error)) {
        return false;
    }
    if (!moveAxis(target.y, 'y', generation, error)) {
        return false;
    }

    const Position reached = position();
    logger_.info("stage reached (" + std::to_string(reached.x) + ", " +
                 std::to_string(reached.y) + ")");
    return true;
}

Position PositioningController::position() const {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return position_;
}

bool PositioningController::isHomed() const {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return homed_;
}

void PositioningController::invalidateHoming() {
    std::lock_guard<std::mutex> lock(positionMutex_);
    homed_ = false;
}

void PositioningController::cancelPending() {
    abort_.store(true, std::memory_order_release);
    {
        // Increment under positionMutex_ so a step cannot pass the generation
        // check and then write a coordinate after cancellation.
        std::lock_guard<std::mutex> lock(positionMutex_);
        generation_.fetch_add(1, std::memory_order_acq_rel);
    }
    jobs_.clear();
}

void PositioningController::clearAbort() {
    abort_.store(false, std::memory_order_release);
}

void PositioningController::run() {
    MoveJob job;
    while (jobs_.pop(job)) {
        setBusy(true);

        if (abort_.load(std::memory_order_acquire) ||
            job.generation != generation_.load(std::memory_order_acquire)) {
            publish(DeviceEventType::MoveAborted, job.cycleId,
                    "move aborted before it started");
            setBusy(false);
            continue;
        }

        std::string error;
        if (moveTo(job.target, error)) {
            publish(DeviceEventType::MoveCompleted, job.cycleId, "");
        } else {
            publish(DeviceEventType::MoveFailed, job.cycleId, error);
        }

        setBusy(false);
    }
}

bool PositioningController::moveAxis(int target, char axisName,
                                     std::uint64_t generation,
                                     std::string& error) {
    // Complexity: O(|target - current| / stepSize) iterations. The mutex is
    // held for one check-and-write step, never while logging.
    while (true) {
        bool reached = false;
        int next = 0;
        {
            std::lock_guard<std::mutex> lock(positionMutex_);

            if (abort_.load(std::memory_order_acquire) ||
                generation_.load(std::memory_order_acquire) != generation) {
                error = "movement aborted";
                return false;
            }

            int& current = (axisName == 'x') ? position_.x : position_.y;
            if (current == target) {
                reached = true;
            } else {
                const int remaining = target - current;
                const int step = std::min(config_.stepSize, std::abs(remaining));
                current += (remaining > 0) ? step : -step;
                next = current;
            }
        }

        if (reached) {
            return true;
        }
        logger_.debug(std::string("axis ") + axisName + " -> " +
                      std::to_string(next));
    }
}

void PositioningController::publish(DeviceEventType type,
                                    std::uint64_t cycleId,
                                    const std::string& message) {
    if (events_ == nullptr) {
        return;
    }

    DeviceEvent event;
    event.type = type;
    event.cycleId = cycleId;
    event.message = message;
    if (!events_->tryPush(std::move(event))) {
        logger_.warning("event queue is full or closed; dropped positioning event");
    }
}

void PositioningController::setBusy(bool busy) {
    {
        std::lock_guard<std::mutex> lock(idleMutex_);
        busy_ = busy;
    }
    if (!busy) {
        idleCv_.notify_all();
    }
}

} // namespace medsync
