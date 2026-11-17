#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "medsync/BoundedQueue.hpp"
#include "medsync/Config.hpp"
#include "medsync/DeviceEvent.hpp"
#include "medsync/DeviceState.hpp"
#include "medsync/PositioningController.hpp"
#include "medsync/SampleHandler.hpp"
#include "medsync/ScanningModule.hpp"
#include "medsync/Types.hpp"

namespace medsync {

class Logger;

// Orchestrates the simulated device and is the only writer of DeviceState.
//
// Threading model:
//   * console thread: home/load/run/estop/reset/shutdown commands
//   * supervisor thread: consumes DeviceEvents, performs state transitions,
//     and submits the next job
//   * positioning worker: performs queued moves
//   * scanning worker: performs queued scans
//
// All device state and the active sample are guarded by mutex_. Worker threads
// only publish events; they never touch controller state.
class DeviceController {
public:
    DeviceController(Config config, Logger& logger);
    ~DeviceController();

    DeviceController(const DeviceController&) = delete;
    DeviceController& operator=(const DeviceController&) = delete;

    // Validates configuration and starts the three background threads.
    bool initialise(std::string& error);

    // Idle -> Homing -> Ready. Synchronous: homing is an operator action and
    // completes before this returns.
    bool home(std::string& error);

    // Requires Ready. Registers one sample; runCycle() starts the work.
    bool loadSample(const std::string& id, const Position& target,
                    std::string& error);

    // Requires Ready and a loaded sample. Submits a move and returns
    // immediately; the supervisor finishes the cycle.
    bool runCycle(std::string& error);

    // Safety actions. Emergency stop can never be rejected: it latches first
    // and then forces the state, bypassing the normal transition table.
    bool emergencyStop(std::string& error);
    bool releaseEmergencyStop(std::string& error);
    bool reset(std::string& error);

    bool shutdown(std::string& error);

    // Test and console helpers.
    bool waitForState(DeviceState expected, std::chrono::milliseconds timeout);
    bool waitForCycleCompletion(std::chrono::milliseconds timeout);

    DeviceState state() const;
    bool hasActiveSample() const;
    std::optional<Sample> activeSample() const;
    Position position() const;
    bool isHomed() const;

    std::string statusText() const;

private:
    void supervisorLoop();
    void handleEvent(const DeviceEvent& event);

    bool requireInitialisedLocked(std::string& error) const;
    bool requireStateLocked(DeviceState expected, const std::string& action,
                            std::string& error) const;
    bool transitionToLocked(DeviceState next, std::string& error);
    void notifyStateChangedLocked();

    Config config_;
    Logger& logger_;

    mutable std::mutex mutex_;
    std::condition_variable stateChanged_;
    DeviceState state_{DeviceState::Idle};
    PositioningController positioning_;
    ScanningModule scanner_;
    SampleHandler samples_;
    BoundedQueue<DeviceEvent> events_{16};
    std::thread supervisor_;
    bool initialised_{false};
    bool startFailed_{false};
    bool estopLatched_{false};
    std::uint64_t nextCycleId_{0};
    std::uint64_t activeCycleId_{0};
};

} // namespace medsync
