#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "medsync/BoundedQueue.hpp"
#include "medsync/Config.hpp"
#include "medsync/DeviceEvent.hpp"
#include "medsync/Types.hpp"

namespace medsync {

class Logger;

// Simulated two-axis stage.
//
// The stage position is owned by this class and guarded by positionMutex_.
// home() is a synchronous operator action; moveTo() is executed by the worker
// thread so that the console never blocks on movement. submitMove() is the only
// way the controller starts a move.
class PositioningController {
public:
    PositioningController(const Config& config, Logger& logger);
    ~PositioningController();

    PositioningController(const PositioningController&) = delete;
    PositioningController& operator=(const PositioningController&) = delete;

    void start(BoundedQueue<DeviceEvent>& events);
    void shutdown();

    // Waits until the worker is not executing a job. Used before a reset clears
    // an abort request.
    bool waitUntilIdle(std::chrono::milliseconds timeout);

    bool home(std::string& error);
    bool submitMove(const Position& target, std::uint64_t cycleId,
                    std::string& error);

    // Synchronous move used by the worker and by unit tests. Bypasses the job
    // queue; production code goes through submitMove().
    bool moveTo(const Position& target, std::string& error);

    Position position() const;
    bool isHomed() const;

    // After an emergency stop the stage must be homed again before moving.
    void invalidateHoming();

    // Emergency-stop support: discard queued moves and make the current move
    // stop at the next step boundary.
    void cancelPending();
    void clearAbort();

private:
    struct MoveJob {
        Position target;
        std::uint64_t cycleId{0};
        std::uint64_t generation{0};
    };

    void run();
    bool moveAxis(int target, char axisName, std::uint64_t generation,
                  std::string& error);
    void waitForStepDelay();
    void publish(DeviceEventType type, std::uint64_t cycleId,
                 const std::string& message);
    void setBusy(bool busy);

    Config config_;
    Logger& logger_;
    BoundedQueue<MoveJob> jobs_{4};
    BoundedQueue<DeviceEvent>* events_{nullptr};
    std::thread worker_;
    std::atomic<bool> abort_{false};
    std::atomic<std::uint64_t> generation_{0};

    // Guards the wait only. cancelPending() takes this mutex before setting
    // abort_, which prevents a lost wake between the predicate check and sleep.
    std::mutex delayMutex_;
    std::condition_variable delayCv_;

    mutable std::mutex positionMutex_;
    Position position_{};
    bool homed_{false};

    std::mutex idleMutex_;
    std::condition_variable idleCv_;
    bool busy_{false};
};

} // namespace medsync
