#include "medsync/SampleHandler.hpp"

#include <cctype>
#include <string>

#include "medsync/Logger.hpp"

namespace medsync {
namespace {

std::string describeTarget(const Position& position) {
    return "(" + std::to_string(position.x) + ", " + std::to_string(position.y) +
           ")";
}

} // namespace

SampleHandler::SampleHandler(const Config& config, Logger& logger)
    : config_(config), logger_(logger) {}

bool SampleHandler::accept(const Sample& sample, std::string& error) {
    if (active_.has_value()) {
        error = "sample '" + active_->id + "' is already loaded";
        return false;
    }
    if (sample.id.empty()) {
        error = "sample id must not be empty";
        return false;
    }
    if (sample.id.size() > config_.maxSampleIdLength) {
        error = "sample id exceeds " + std::to_string(config_.maxSampleIdLength) +
                " characters";
        return false;
    }
    for (const char character : sample.id) {
        if (!isValidIdCharacter(character)) {
            error = "sample id may contain only letters, digits, and hyphens";
            return false;
        }
    }
    if (!config_.contains(sample.target)) {
        error = "sample target " + describeTarget(sample.target) +
                " is outside the stage bounds";
        return false;
    }

    active_ = sample;
    active_->status = SampleStatus::Received;
    logger_.info("sample '" + sample.id + "' accepted for target " +
                 describeTarget(sample.target));
    return true;
}

bool SampleHandler::hasActive() const noexcept {
    return active_.has_value();
}

const std::optional<Sample>& SampleHandler::active() const noexcept {
    return active_;
}

void SampleHandler::markPositioned() {
    if (!active_.has_value()) {
        logger_.warning("markPositioned ignored: no active sample");
        return;
    }
    active_->status = SampleStatus::Positioned;
    logger_.debug("sample '" + active_->id + "' positioned");
}

void SampleHandler::markScanning() {
    if (!active_.has_value()) {
        logger_.warning("markScanning ignored: no active sample");
        return;
    }
    active_->status = SampleStatus::Scanning;
    logger_.debug("sample '" + active_->id + "' scanning");
}

void SampleHandler::complete(const ScanResult& result) {
    if (!active_.has_value()) {
        logger_.warning("complete ignored: no active sample");
        return;
    }

    active_->status = SampleStatus::Completed;
    logger_.info("sample '" + active_->id + "' completed with quality " +
                 std::to_string(result.qualityScore));
    active_.reset();
}

void SampleHandler::fail(const std::string& reason) {
    if (!active_.has_value()) {
        return;
    }

    logger_.warning("sample '" + active_->id + "' failed: " + reason);
    active_.reset();
}

bool SampleHandler::isValidIdCharacter(char character) noexcept {
    const auto value = static_cast<unsigned char>(character);
    return std::isalnum(value) != 0 || character == '-';
}

} // namespace medsync
