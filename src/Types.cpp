#include "medsync/Types.hpp"

namespace medsync {

bool operator==(const Position& lhs, const Position& rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool operator!=(const Position& lhs, const Position& rhs) noexcept {
    return !(lhs == rhs);
}

std::string_view to_string(SampleStatus status) noexcept {
    switch (status) {
    case SampleStatus::Received:
        return "Received";
    case SampleStatus::Positioned:
        return "Positioned";
    case SampleStatus::Scanning:
        return "Scanning";
    case SampleStatus::Completed:
        return "Completed";
    case SampleStatus::Failed:
        return "Failed";
    }
    return "Unknown";
}

} // namespace medsync
