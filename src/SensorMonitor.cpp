#include "medsync/SensorMonitor.hpp"

#include <string>

#include "medsync/Logger.hpp"

namespace medsync {

SensorMonitor::SensorMonitor(const Config& config, Logger& logger)
    : config_(config), logger_(logger) {}

SensorMonitor::~SensorMonitor() {
    shutdown();
}

void SensorMonitor::start() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (worker_.joinable() || stop_) {
            return;
        }
        // The new thread blocks on mutex_ until this scope ends, so it cannot
        // observe a half-initialised object.
        worker_ = std::thread(&SensorMonitor::run, this);
    }
    logger_.info("sensor monitor started; period " +
                 std::to_string(config_.sensorPeriod.count()) + " ms");
}

void SensorMonitor::shutdown() {
    bool wasRunning = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
        wasRunning = worker_.joinable();
    }

    stopCv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    if (wasRunning) {
        logger_.info("sensor monitor stopped");
    }
}

SensorReading SensorMonitor::latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
}

bool SensorMonitor::waitForReadingAfter(std::uint64_t sequence,
                                        std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return readingCv_.wait_for(lock, timeout, [this, sequence] {
        return latest_.sequence > sequence;
    });
}

SensorReading SensorMonitor::sampleOnce() {
    SensorReading reading;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        reading = generateLocked();
    }
    readingCv_.notify_all();
    return reading;
}

void SensorMonitor::run() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (!stop_) {
        generateLocked();
        lock.unlock();
        readingCv_.notify_all();
        lock.lock();

        if (stopCv_.wait_for(lock, config_.sensorPeriod,
                             [this] { return stop_; })) {
            break;
        }
    }
}

SensorReading SensorMonitor::generateLocked() {
    const auto nextUnit = [this] {
        rngState_ = rngState_ * 1664525u + 1013904223u;
        // The low bits of an LCG are weak; take the middle 16 bits.
        return static_cast<double>((rngState_ >> 8) & 0xFFFFu) / 65535.0;
    };

    SensorReading reading;
    reading.sequence = ++sequence_;
    reading.temperatureC = 33.0 + 5.0 * nextUnit();
    reading.forceN = 12.0 * nextUnit();
    latest_ = reading;
    return reading;
}

} // namespace medsync
