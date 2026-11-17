#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include "medsync/Types.hpp"

namespace medsync {

// Immutable-at-runtime configuration for the simulation. Passed by value into
// components so tests can use small bounds and fast settings without globals.
struct Config {
    int minCoordinate{-100};
    int maxCoordinate{100};
    int stepSize{10};
    Position homePosition{0, 0};
    std::size_t maxSampleIdLength{32};

    // Simulated timing. Zero step and scan delays keep unit tests fast; the
    // defaults give the console visible motion and scan time.
    std::chrono::milliseconds stepDelay{5};
    std::chrono::milliseconds scanDuration{50};
    std::chrono::milliseconds sensorPeriod{100};

    // True when both axes are inside [minCoordinate, maxCoordinate].
    bool contains(const Position& position) const noexcept;

    // Returns false and fills `error` when the configuration is unusable.
    bool validate(std::string& error) const;
};

} // namespace medsync
