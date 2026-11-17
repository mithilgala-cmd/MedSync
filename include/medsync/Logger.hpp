#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <ostream>
#include <string_view>

namespace medsync {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
};

std::string_view to_string(LogLevel level) noexcept;

// Thread-safe event logger. All messages are written through a single mutex,
// which keeps lines intact when worker threads start logging in Milestone 2.
// The clock is steady_clock, so timestamps are monotonic and unaffected by
// system time changes.
class Logger {
public:
    explicit Logger(std::ostream& out);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, std::string_view message);

    void debug(std::string_view message);
    void info(std::string_view message);
    void warning(std::string_view message);
    void error(std::string_view message);

    std::size_t entryCount() const;

private:
    std::ostream& out_;
    std::chrono::steady_clock::time_point start_;
    mutable std::mutex mutex_;
    std::size_t entryCount_{0};
};

} // namespace medsync
