#include "medsync/ScanningModule.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "medsync/Logger.hpp"

namespace medsync {
namespace {

// FNV-1a: small, deterministic across platforms, and good enough to derive a
// fake quality score. std::hash is not guaranteed to be stable across runs or
// standard library versions, so it is not used here.
std::uint32_t fnv1a(std::string_view text) noexcept {
    std::uint32_t hash = 2166136261u;
    for (const char character : text) {
        hash ^= static_cast<unsigned char>(character);
        hash *= 16777619u;
    }
    return hash;
}

} // namespace

ScanningModule::ScanningModule(Logger& logger) : logger_(logger) {}

ScanningModule::~ScanningModule() {
    shutdown();
}

void ScanningModule::start(BoundedQueue<DeviceEvent>& events) {
    if (worker_.joinable()) {
        return;
    }
    events_ = &events;
    worker_ = std::thread(&ScanningModule::run, this);
}

void ScanningModule::shutdown() {
    abort_.store(true, std::memory_order_release);
    jobs_.close();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool ScanningModule::waitUntilIdle(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(idleMutex_);
    return idleCv_.wait_for(lock, timeout, [this] { return !busy_; });
}

bool ScanningModule::submitScan(const Sample& sample, std::uint64_t cycleId,
                                std::string& error) {
    if (!worker_.joinable()) {
        error = "scanning worker is not running";
        return false;
    }

    const std::uint64_t generation =
        generation_.load(std::memory_order_acquire);
    if (!jobs_.tryPush(ScanJob{sample, cycleId, generation})) {
        error = "scanning queue is full or closed";
        return false;
    }
    return true;
}

ScanResult ScanningModule::scan(const Sample& sample) {
    ScanResult result;
    result.sampleId = sample.id;

    if (sample.id.empty()) {
        result.successful = false;
        result.note = "scan rejected: empty sample id";
        logger_.warning(result.note);
        return result;
    }

    result.successful = true;
    result.qualityScore = 80 + static_cast<int>(fnv1a(sample.id) % 21u);
    result.note = "synthetic scan record";

    logger_.info("scan completed for sample '" + sample.id +
                 "' (synthetic quality " + std::to_string(result.qualityScore) +
                 ")");
    return result;
}

void ScanningModule::cancelPending() {
    abort_.store(true, std::memory_order_release);
    // Invalidate jobs that a worker may already have popped but not started.
    // Without this, reset() could clear abort_ and let such a job run.
    generation_.fetch_add(1, std::memory_order_acq_rel);
    jobs_.clear();
}

void ScanningModule::clearAbort() {
    abort_.store(false, std::memory_order_release);
}

void ScanningModule::run() {
    ScanJob job;
    while (jobs_.pop(job)) {
        setBusy(true);

        if (abort_.load(std::memory_order_acquire) ||
            job.generation != generation_.load(std::memory_order_acquire)) {
            ScanResult aborted;
            aborted.sampleId = job.sample.id;
            aborted.successful = false;
            aborted.note = "scan aborted";
            publish(DeviceEventType::ScanFailed, job.cycleId, aborted,
                    aborted.note);
            setBusy(false);
            continue;
        }

        const ScanResult result = scan(job.sample);
        if (result.successful) {
            publish(DeviceEventType::ScanCompleted, job.cycleId, result, "");
        } else {
            publish(DeviceEventType::ScanFailed, job.cycleId, result,
                    result.note);
        }

        setBusy(false);
    }
}

void ScanningModule::publish(DeviceEventType type, std::uint64_t cycleId,
                             const ScanResult& result,
                             const std::string& message) {
    if (events_ == nullptr) {
        return;
    }

    DeviceEvent event;
    event.type = type;
    event.cycleId = cycleId;
    event.scan = result;
    event.message = message;
    if (!events_->tryPush(std::move(event))) {
        logger_.warning("event queue is full or closed; dropped scan event");
    }
}

void ScanningModule::setBusy(bool busy) {
    {
        std::lock_guard<std::mutex> lock(idleMutex_);
        busy_ = busy;
    }
    if (!busy) {
        idleCv_.notify_all();
    }
}

} // namespace medsync
