#pragma once

#include <string>

#include "medsync/Config.hpp"
#include "medsync/DeviceState.hpp"
#include "medsync/PositioningController.hpp"
#include "medsync/SampleHandler.hpp"
#include "medsync/ScanningModule.hpp"
#include "medsync/Types.hpp"

namespace medsync {

class Logger;

// Orchestrates the simulated device. This is the only class that writes
// DeviceState, and the only entry point the console layer uses.
//
// Milestone 1 is synchronous: home(), runCycle(), and the safety commands
// complete before returning. Milestone 2 keeps the same public API but adds
// worker threads and an asynchronous runCycle().
class DeviceController {
public:
    DeviceController(Config config, Logger& logger);

    DeviceController(const DeviceController&) = delete;
    DeviceController& operator=(const DeviceController&) = delete;

    bool initialise(std::string& error);

    // Idle -> Homing -> Ready.
    bool home(std::string& error);

    // Requires Ready. Registers one sample; runCycle() performs the work.
    bool loadSample(const std::string& id, const Position& target,
                    std::string& error);

    // Requires Ready and a loaded sample. Positioning -> Scanning -> Ready.
    bool runCycle(std::string& error);

    // Safety actions. Emergency stop can never be rejected: it latches first
    // and then forces the state, bypassing the normal transition table.
    bool emergencyStop(std::string& error);
    bool releaseEmergencyStop(std::string& error);
    bool reset(std::string& error);

    bool shutdown(std::string& error);

    DeviceState state() const noexcept;
    const PositioningController& positioning() const noexcept;
    const SampleHandler& samples() const noexcept;

    std::string statusText() const;

private:
    bool requireInitialised(std::string& error) const;
    bool requireState(DeviceState expected, const std::string& action,
                      std::string& error) const;
    bool transitionTo(DeviceState next, std::string& error);

    Config config_;
    Logger& logger_;
    DeviceState state_{DeviceState::Idle};
    PositioningController positioning_;
    ScanningModule scanner_;
    SampleHandler samples_;
    bool initialised_{false};
    bool estopLatched_{false};
};

} // namespace medsync
