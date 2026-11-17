#include "medsync/Config.hpp"

namespace medsync {

bool Config::contains(const Position& position) const noexcept {
    return position.x >= minCoordinate && position.x <= maxCoordinate &&
           position.y >= minCoordinate && position.y <= maxCoordinate;
}

bool Config::validate(std::string& error) const {
    if (minCoordinate >= maxCoordinate) {
        error = "minCoordinate must be strictly less than maxCoordinate";
        return false;
    }
    if (stepSize <= 0) {
        error = "stepSize must be positive";
        return false;
    }
    if (stepSize > (maxCoordinate - minCoordinate)) {
        error = "stepSize must not exceed the coordinate range";
        return false;
    }
    if (!contains(homePosition)) {
        error = "homePosition must be inside the coordinate bounds";
        return false;
    }
    if (maxSampleIdLength == 0) {
        error = "maxSampleIdLength must be at least 1";
        return false;
    }
    if (maxSampleIdLength > 128) {
        error = "maxSampleIdLength must not exceed 128";
        return false;
    }
    return true;
}

} // namespace medsync
