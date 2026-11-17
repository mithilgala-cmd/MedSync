#include "medsync/DeviceController.hpp"

#include <exception>
#include <sstream>
#include <string>
#include <utility>

#include "medsync/Logger.hpp"

namespace medsync {

DeviceController::DeviceController(Config config, Logger& logger)
    : config_(std::move(config)),
      logger_(logger),
      positioning_(config_, logger_),
      scanner_(logger_),
      samples_(config_, logger_) {}

DeviceController::~DeviceController() {
    std::string ignored;
    (void)shutdown(ignored);
}

bool DeviceController::initialise(std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialised_) {
        error = "device is already initialised";
        return false;
    }
    if (startFailed_) {
        error = "device cannot be initialised after a thread startup failure";
        return false;
    }
    if (state_ != DeviceState::Idle) {
        error = "device cannot be initialised from state " +
                std::string(to_string(state_));
        return false;
    }
    if (!config_.validate(error)) {
        return false;
    }

    try {
        positioning_.start(events_);
        scanner_.start(events_);
        supervisor_ = std::thread(&DeviceController::supervisorLoop, this);
    } catch (const std::exception& exception) {
        startFailed_ = true;
        error = std::string("failed to start worker threads: ") +
                exception.what();
        positioning_.shutdown();
        scanner_.shutdown();
        events_.close();
        if (supervisor_.joinable()) {
            supervisor_.join();
        }
        logger_.error(error);
        return false;
    }

    initialised_ = true;
    logger_.info("device initialised; supervisor and worker threads started");
    return true;
}

bool DeviceController::home(std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!requireInitialisedLocked(error) ||
        !requireStateLocked(DeviceState::Idle, "homing", error)) {
        return false;
    }
    if (!transitionToLocked(DeviceState::Homing, error)) {
        return false;
    }

    if (!positioning_.home(error)) {
        (void)transitionToLocked(DeviceState::Fault, error);
        notifyStateChangedLocked();
        return false;
    }
    if (!transitionToLocked(DeviceState::Ready, error)) {
        return false;
    }

    notifyStateChangedLocked();
    return true;
}

bool DeviceController::loadSample(const std::string& id,
                                  const Position& target,
                                  std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!requireInitialisedLocked(error) ||
        !requireStateLocked(DeviceState::Ready, "loading a sample", error)) {
        return false;
    }

    const Sample sample{id, target, SampleStatus::Received};
    if (!samples_.accept(sample, error)) {
        return false;
    }

    logger_.info("sample '" + id + "' loaded; run the cycle to process it");
    return true;
}

bool DeviceController::runCycle(std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!requireInitialisedLocked(error) ||
        !requireStateLocked(DeviceState::Ready, "running a cycle", error)) {
        return false;
    }
    if (!samples_.hasActive()) {
        error = "no sample is loaded";
        return false;
    }

    const Position target = samples_.active()->target;
    const std::uint64_t cycleId = nextCycleId_ + 1;

    if (!positioning_.submitMove(target, cycleId, error)) {
        return false;
    }

    nextCycleId_ = cycleId;
    activeCycleId_ = cycleId;
    if (!transitionToLocked(DeviceState::Positioning, error)) {
        return false;
    }

    logger_.info("cycle " + std::to_string(cycleId) + " submitted for sample '" +
                 samples_.active()->id + "'");
    notifyStateChangedLocked();
    return true;
}

bool DeviceController::emergencyStop(std::string& /*error*/) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Latch first. A stop command must never depend on state validation.
    estopLatched_ = true;

    if (state_ == DeviceState::EmergencyStop) {
        logger_.info("emergency stop already active");
        return true;
    }
    if (state_ == DeviceState::ShuttingDown) {
        logger_.warning(
            "emergency stop requested during shutdown; motion is already stopped");
        return true;
    }

    const std::string previous{to_string(state_)};
    state_ = DeviceState::EmergencyStop;
    logger_.warning("EMERGENCY STOP: state " + previous + " -> EmergencyStop");

    samples_.fail("emergency stop");
    positioning_.invalidateHoming();
    positioning_.cancelPending();
    scanner_.cancelPending();
    notifyStateChangedLocked();
    return true;
}

bool DeviceController::releaseEmergencyStop(std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!requireInitialisedLocked(error)) {
        return false;
    }
    if (state_ != DeviceState::EmergencyStop) {
        error = "no emergency stop is active";
        return false;
    }
    if (!estopLatched_) {
        logger_.info("emergency stop already released; reset required");
        return true;
    }

    estopLatched_ = false;
    logger_.info("emergency stop released; reset required before homing");
    notifyStateChangedLocked();
    return true;
}

bool DeviceController::reset(std::string& error) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!requireInitialisedLocked(error)) {
            return false;
        }
        if (state_ != DeviceState::Fault &&
            state_ != DeviceState::EmergencyStop) {
            error = "reset requires the Fault or EmergencyStop state";
            return false;
        }
        if (state_ == DeviceState::EmergencyStop && estopLatched_) {
            error = "emergency stop is still latched; release it first";
            return false;
        }
    }

    // Let an aborted worker finish before clearing the abort flag. The mutex is
    // released here so the worker can publish its final event and go idle.
    const auto timeout = std::chrono::milliseconds(2000);
    const bool positioningIdle = positioning_.waitUntilIdle(timeout);
    const bool scanningIdle = scanner_.waitUntilIdle(timeout);
    if (!positioningIdle || !scanningIdle) {
        error = "reset aborted: a worker did not become idle";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != DeviceState::Fault &&
        state_ != DeviceState::EmergencyStop) {
        error = "reset aborted: device state changed";
        return false;
    }
    if (state_ == DeviceState::EmergencyStop && estopLatched_) {
        error = "emergency stop is still latched; release it first";
        return false;
    }

    if (!transitionToLocked(DeviceState::Idle, error)) {
        return false;
    }

    samples_.fail("device reset");
    positioning_.invalidateHoming();
    positioning_.clearAbort();
    scanner_.clearAbort();
    activeCycleId_ = 0;
    notifyStateChangedLocked();
    logger_.info("reset complete; the stage must be homed before use");
    return true;
}

bool DeviceController::shutdown(std::string& error) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (state_ == DeviceState::ShuttingDown) {
        return true;
    }
    if (!canTransition(state_, DeviceState::ShuttingDown)) {
        error = "cannot shut down from state " +
                std::string(to_string(state_));
        return false;
    }

    const std::string previous{to_string(state_)};
    state_ = DeviceState::ShuttingDown;
    samples_.fail("shutdown");
    logger_.info("shutdown requested: " + previous + " -> ShuttingDown");
    notifyStateChangedLocked();
    lock.unlock();

    positioning_.shutdown();
    scanner_.shutdown();
    events_.close();
    if (supervisor_.joinable()) {
        supervisor_.join();
    }

    logger_.info("shutdown complete; worker threads joined");
    return true;
}

bool DeviceController::waitForState(DeviceState expected,
                                    std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return stateChanged_.wait_for(lock, timeout,
                                  [this, expected] { return state_ == expected; });
}

bool DeviceController::waitForCycleCompletion(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return stateChanged_.wait_for(lock, timeout, [this] {
        return state_ == DeviceState::Ready && !samples_.hasActive();
    });
}

DeviceState DeviceController::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool DeviceController::hasActiveSample() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return samples_.hasActive();
}

std::optional<Sample> DeviceController::activeSample() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return samples_.active();
}

Position DeviceController::position() const {
    return positioning_.position();
}

bool DeviceController::isHomed() const {
    return positioning_.isHomed();
}

std::string DeviceController::statusText() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream out;
    out << "state=" << to_string(state_) << " position=("
        << positioning_.position().x << ", " << positioning_.position().y
        << ") homed=" << (positioning_.isHomed() ? "yes" : "no")
        << " sample=";

    if (samples_.hasActive()) {
        out << samples_.active()->id << " ("
            << to_string(samples_.active()->status) << ")";
    } else {
        out << "none";
    }
    return out.str();
}

void DeviceController::supervisorLoop() {
    DeviceEvent event;
    while (events_.pop(event)) {
        handleEvent(event);
    }
}

void DeviceController::handleEvent(const DeviceEvent& event) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!initialised_ || state_ == DeviceState::ShuttingDown) {
        return;
    }

    switch (event.type) {
    case DeviceEventType::MoveCompleted: {
        if (state_ != DeviceState::Positioning ||
            event.cycleId != activeCycleId_) {
            logger_.debug("ignoring stale move completion");
            return;
        }

        samples_.markPositioned();
        std::string error;
        if (!transitionToLocked(DeviceState::Scanning, error)) {
            return;
        }

        if (!samples_.hasActive()) {
            logger_.error("move completed without an active sample");
            (void)transitionToLocked(DeviceState::Fault, error);
            notifyStateChangedLocked();
            return;
        }

        if (!scanner_.submitScan(*samples_.active(), activeCycleId_, error)) {
            samples_.fail("scan dispatch failed: " + error);
            (void)transitionToLocked(DeviceState::Fault, error);
            logger_.error(error);
        }
        notifyStateChangedLocked();
        break;
    }

    case DeviceEventType::MoveAborted:
    case DeviceEventType::MoveFailed: {
        if (state_ != DeviceState::Positioning ||
            event.cycleId != activeCycleId_) {
            logger_.debug("ignoring stale positioning failure");
            return;
        }

        const std::string reason =
            event.message.empty() ? "positioning failed" : event.message;
        samples_.fail(reason);
        positioning_.invalidateHoming();
        positioning_.cancelPending();
        std::string error;
        (void)transitionToLocked(DeviceState::Fault, error);
        notifyStateChangedLocked();
        break;
    }

    case DeviceEventType::ScanCompleted: {
        if (state_ != DeviceState::Scanning ||
            event.cycleId != activeCycleId_) {
            logger_.debug("ignoring stale scan completion");
            return;
        }

        std::string error;
        if (event.scan.successful) {
            samples_.complete(event.scan);
            (void)transitionToLocked(DeviceState::Ready, error);
        } else {
            samples_.fail(event.scan.note.empty() ? "scan failed"
                                                  : event.scan.note);
            scanner_.cancelPending();
            (void)transitionToLocked(DeviceState::Fault, error);
        }
        notifyStateChangedLocked();
        break;
    }

    case DeviceEventType::ScanFailed: {
        if (state_ != DeviceState::Scanning ||
            event.cycleId != activeCycleId_) {
            logger_.debug("ignoring stale scan failure");
            return;
        }

        samples_.fail(event.message.empty() ? "scan failed" : event.message);
        scanner_.cancelPending();
        std::string error;
        (void)transitionToLocked(DeviceState::Fault, error);
        notifyStateChangedLocked();
        break;
    }
    }
}

bool DeviceController::requireInitialisedLocked(std::string& error) const {
    if (initialised_) {
        return true;
    }
    error = "device is not initialised";
    return false;
}

bool DeviceController::requireStateLocked(DeviceState expected,
                                          const std::string& action,
                                          std::string& error) const {
    if (state_ == expected) {
        return true;
    }
    error = action + " requires state " + std::string(to_string(expected)) +
            " (current " + std::string(to_string(state_)) + ")";
    return false;
}

bool DeviceController::transitionToLocked(DeviceState next, std::string& error) {
    if (!canTransition(state_, next)) {
        error = "invalid transition from " + std::string(to_string(state_)) +
                " to " + std::string(to_string(next));
        logger_.warning(error);
        return false;
    }

    const std::string transition = std::string(to_string(state_)) + " -> " +
                                   std::string(to_string(next));
    state_ = next;
    logger_.info("state: " + transition);
    return true;
}

void DeviceController::notifyStateChangedLocked() {
    stateChanged_.notify_all();
}

} // namespace medsync
