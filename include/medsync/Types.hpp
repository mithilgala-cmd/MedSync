#pragma once

#include <string>
#include <string_view>

namespace medsync {

// Two-axis stage position in millimetres. The simulation intentionally omits a
// Z axis; adding one later only affects this type and the bounds check.
struct Position {
    int x{0};
    int y{0};
};

bool operator==(const Position& lhs, const Position& rhs) noexcept;
bool operator!=(const Position& lhs, const Position& rhs) noexcept;

enum class SampleStatus {
    Received,
    Positioned,
    Scanning,
    Completed,
    Failed,
};

std::string_view to_string(SampleStatus status) noexcept;

struct Sample {
    std::string id;
    Position target;
    SampleStatus status{SampleStatus::Received};
};

// Synthetic scan output. qualityScore is derived deterministically from the
// sample id for reproducible tests; it is not a measurement of anything.
struct ScanResult {
    std::string sampleId;
    bool successful{false};
    int qualityScore{0};
    std::string note;
};

} // namespace medsync
