#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include "medsync/Config.hpp"
#include "medsync/Types.hpp"

namespace medsync {

class Logger;

// Periodic source of synthetic sensor readings.
//
// The monitor owns one worker thread and a small snapshot guarded by mutex_.
// It is a pull source, not an event publisher: a reading every 100 ms would
// flood the worker-to-supervisor event queue. The controller reads latest()
// for status, and the future fault monitor can evaluate thresholds against it.
// Readings are deterministic for a given start state but are not measurements.
class SensorMonitor {
public:
    SensorMonitor(const Config& config, Logger& logger);
    ~SensorMonitor();

    SensorMonitor(const SensorMonitor&) = delete;
    SensorMonitor& operator=(const SensorMonitor&) = delete;

    // One-shot lifecycle, matching the worker components.
    void start();
    void shutdown();

    SensorReading latest() const;

    // Blocks until a reading newer than `sequence` exists, or the timeout
    // expires. Used by tests and by future threshold logic.
    bool waitForReadingAfter(std::uint64_t sequence,
                             std::chrono::milliseconds timeout);

    // Generates one reading immediately without sleeping. Safe to call without
    // start(); the periodic worker uses the same generator.
    SensorReading sampleOnce();

private:
    void run();
    SensorReading generateLocked();

    Config config_;
    Logger& logger_;

    mutable std::mutex mutex_;
    std::condition_variable readingCv_;
    std::condition_variable stopCv_;
    std::thread worker_;
    bool stop_{false};

    // Deterministic LCG state. std::rand is global and not reproducible across
    // runs or threads, so it is deliberately not used.
    std::uint32_t rngState_{0x2545F491u};
    std::uint64_t sequence_{0};
    SensorReading latest_{};
};

} // namespace medsync
