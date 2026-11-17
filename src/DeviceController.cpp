#include "medsync/DeviceController.hpp"

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

bool DeviceController::initialise(std::string& error) {
    if (initialised_) {
        error = "device is already initialised";
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

    initialised_ = true;
    logger_.info("device initialised; state=Idle");
    return true;
}

bool DeviceController::home(std::string& error) {
    if (!requireInitialised(error) ||
        !requireState(DeviceState::Idle, "homing", error)) {
        return false;
    }
    if (!transitionTo(DeviceState::Homing, error)) {
        return false;
    }

    std::string homingError;
    if (!positioning_.home(homingError)) {
        (void)transitionTo(DeviceState::Fault, error);
        error = homingError;
        return false;
    }
    return transitionTo(DeviceState::Ready, error);
}

bool DeviceController::loadSample(const std::string& id,
                                  const Position& target,
                                  std::string& error) {
    if (!requireInitialised(error) ||
        !requireState(DeviceState::Ready, "loading a sample", error)) {
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
    if (!requireInitialised(error) ||
        !requireState(DeviceState::Ready, "running a cycle", error)) {
        return false;
    }
    if (!samples_.hasActive()) {
        error = "no sample is loaded";
        return false;
    }

    if (!transitionTo(DeviceState::Positioning, error)) {
        return false;
    }

    const Position target = samples_.active()->target;
    std::string positioningError;
    if (!positioning_.moveTo(target, positioningError)) {
        // Defensive path: targets are validated on acceptance, so reaching
        // this means the internal state is inconsistent.
        logger_.error("positioning failed: " + positioningError);
        samples_.fail(positioningError);
        (void)transitionTo(DeviceState::Fault, error);
        error = positioningError;
        return false;
    }

    samples_.markPositioned();
    if (!transitionTo(DeviceState::Scanning, error)) {
        return false;
    }

    samples_.markScanning();
    const ScanResult result = scanner_.scan(*samples_.active());
    if (!result.successful) {
        samples_.fail(result.note);
        (void)transitionTo(DeviceState::Fault, error);
        error = result.note;
        return false;
    }

    samples_.complete(result);
    return transitionTo(DeviceState::Ready, error);
}

bool DeviceController::emergencyStop(std::string& /*error*/) {
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
    return true;
}

bool DeviceController::releaseEmergencyStop(std::string& error) {
    if (!requireInitialised(error)) {
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
    return true;
}

bool DeviceController::reset(std::string& error) {
    if (!requireInitialised(error)) {
        return false;
    }
    if (state_ != DeviceState::Fault && state_ != DeviceState::EmergencyStop) {
        error = "reset requires the Fault or EmergencyStop state";
        return false;
    }
    if (state_ == DeviceState::EmergencyStop && estopLatched_) {
        error = "emergency stop is still latched; release it first";
        return false;
    }
    if (!transitionTo(DeviceState::Idle, error)) {
        return false;
    }

    // Fault causes are tracked from Milestone 4. Until then, reset is only
    // reachable from a cleared emergency stop or an internally raised fault.
    samples_.fail("device reset");
    positioning_.invalidateHoming();
    logger_.info("reset complete; the stage must be homed before use");
    return true;
}

bool DeviceController::shutdown(std::string& error) {
    if (state_ == DeviceState::ShuttingDown) {
        return true;
    }
    if (!transitionTo(DeviceState::ShuttingDown, error)) {
        return false;
    }

    samples_.fail("shutdown");
    logger_.info("shutdown complete; no worker threads exist in Milestone 1");
    return true;
}

DeviceState DeviceController::state() const noexcept {
    return state_;
}

const PositioningController& DeviceController::positioning() const noexcept {
    return positioning_;
}

const SampleHandler& DeviceController::samples() const noexcept {
    return samples_;
}

std::string DeviceController::statusText() const {
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

bool DeviceController::requireInitialised(std::string& error) const {
    if (initialised_) {
        return true;
    }
    error = "device is not initialised";
    return false;
}

bool DeviceController::requireState(DeviceState expected,
                                    const std::string& action,
                                    std::string& error) const {
    if (state_ == expected) {
        return true;
    }
    error = action + " requires state " + std::string(to_string(expected)) +
            " (current " + std::string(to_string(state_)) + ")";
    return false;
}

bool DeviceController::transitionTo(DeviceState next, std::string& error) {
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

} // namespace medsync
