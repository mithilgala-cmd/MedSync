#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "medsync/BoundedQueue.hpp"
#include "medsync/DeviceEvent.hpp"
#include "medsync/Types.hpp"

namespace medsync {

class Logger;

// Synthetic scanner. The worker thread turns a queued sample into a scan event;
// the controller's supervisor decides what that means for device state.
class ScanningModule {
public:
    explicit ScanningModule(Logger& logger);
    ~ScanningModule();

    ScanningModule(const ScanningModule&) = delete;
    ScanningModule& operator=(const ScanningModule&) = delete;

    void start(BoundedQueue<DeviceEvent>& events);
    void shutdown();
    bool waitUntilIdle(std::chrono::milliseconds timeout);

    bool submitScan(const Sample& sample, std::uint64_t cycleId,
                    std::string& error);

    // Synchronous scan used by the worker and by unit tests.
    ScanResult scan(const Sample& sample);

    void cancelPending();
    void clearAbort();

private:
    struct ScanJob {
        Sample sample;
        std::uint64_t cycleId{0};
        std::uint64_t generation{0};
    };

    void run();
    void publish(DeviceEventType type, std::uint64_t cycleId,
                 const ScanResult& result, const std::string& message);
    void setBusy(bool busy);

    Logger& logger_;
    BoundedQueue<ScanJob> jobs_{4};
    BoundedQueue<DeviceEvent>* events_{nullptr};
    std::thread worker_;
    std::atomic<bool> abort_{false};
    std::atomic<std::uint64_t> generation_{0};

    std::mutex idleMutex_;
    std::condition_variable idleCv_;
    bool busy_{false};
};

} // namespace medsync
