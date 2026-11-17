#include "medsync/ScanningModule.hpp"

#include <cstdint>
#include <string>
#include <string_view>

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

    logger_.info("scan completed for sample '" + sample.id + "' (synthetic quality " +
                 std::to_string(result.qualityScore) + ")");
    return result;
}

} // namespace medsync
