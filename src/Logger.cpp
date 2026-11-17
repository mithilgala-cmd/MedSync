#include "medsync/Logger.hpp"

#include <string>

namespace medsync {

std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "UNKNOWN";
}

Logger::Logger(std::ostream& out)
    : out_(out), start_(std::chrono::steady_clock::now()) {}

void Logger::log(LogLevel level, std::string_view message) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - start_)
                             .count();

    std::lock_guard<std::mutex> lock(mutex_);
    out_ << "[+" << elapsed << " ms] [" << to_string(level) << "] " << message
         << '\n';
    out_.flush();
    ++entryCount_;
}

void Logger::debug(std::string_view message) {
    log(LogLevel::Debug, message);
}

void Logger::info(std::string_view message) {
    log(LogLevel::Info, message);
}

void Logger::warning(std::string_view message) {
    log(LogLevel::Warning, message);
}

void Logger::error(std::string_view message) {
    log(LogLevel::Error, message);
}

std::size_t Logger::entryCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entryCount_;
}

} // namespace medsync
