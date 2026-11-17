#pragma once

#include <optional>
#include <string>

#include "medsync/Config.hpp"
#include "medsync/Types.hpp"

namespace medsync {

class Logger;

// Owns the lifecycle of the sample currently being processed. Milestone 1 is
// single-threaded and holds at most one active sample; Milestone 2 replaces the
// single slot with a bounded, thread-safe queue.
class SampleHandler {
public:
    SampleHandler(const Config& config, Logger& logger);

    // Validates the id and target, then stores the sample. Rejects a second
    // sample while one is already active.
    bool accept(const Sample& sample, std::string& error);

    bool hasActive() const noexcept;
    const std::optional<Sample>& active() const noexcept;

    void markPositioned();
    void markScanning();
    void complete(const ScanResult& result);
    void fail(const std::string& reason);

private:
    static bool isValidIdCharacter(char character) noexcept;

    Config config_;
    Logger& logger_;
    std::optional<Sample> active_;
};

} // namespace medsync
