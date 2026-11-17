#include "medsync/Config.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace medsync {
namespace {

TEST(ConfigTest, DefaultsAreValid) {
    const Config config;
    std::string error;
    EXPECT_TRUE(config.validate(error)) << error;
}

TEST(ConfigTest, RejectsInvertedBounds) {
    Config config;
    config.minCoordinate = 10;
    config.maxCoordinate = 10;
    std::string error;
    EXPECT_FALSE(config.validate(error));
    EXPECT_FALSE(error.empty());
}

TEST(ConfigTest, RejectsNonPositiveStepSize) {
    Config config;
    config.stepSize = 0;
    std::string error;
    EXPECT_FALSE(config.validate(error));
}

TEST(ConfigTest, RejectsStepLargerThanRange) {
    Config config;
    config.stepSize = 1000;
    std::string error;
    EXPECT_FALSE(config.validate(error));
}

TEST(ConfigTest, RejectsHomeOutsideBounds) {
    Config config;
    config.homePosition = Position{500, 0};
    std::string error;
    EXPECT_FALSE(config.validate(error));
}

TEST(ConfigTest, RejectsInvalidSampleIdLimits) {
    Config config;
    config.maxSampleIdLength = 0;
    std::string error;
    EXPECT_FALSE(config.validate(error));

    config.maxSampleIdLength = 129;
    error.clear();
    EXPECT_FALSE(config.validate(error));
}

TEST(ConfigTest, RejectsNegativeDurations) {
    Config config;
    config.stepDelay = std::chrono::milliseconds(-1);
    std::string error;
    EXPECT_FALSE(config.validate(error));

    config.stepDelay = std::chrono::milliseconds(0);
    config.scanDuration = std::chrono::milliseconds(-1);
    error.clear();
    EXPECT_FALSE(config.validate(error));
}

TEST(ConfigTest, RejectsNonPositiveSensorPeriod) {
    Config config;
    config.sensorPeriod = std::chrono::milliseconds(0);
    std::string error;
    EXPECT_FALSE(config.validate(error));
}

TEST(ConfigTest, ContainsChecksBothAxes) {
    const Config config; // [-100, 100] on both axes
    EXPECT_TRUE(config.contains(Position{0, 0}));
    EXPECT_TRUE(config.contains(Position{-100, 100}));
    EXPECT_FALSE(config.contains(Position{101, 0}));
    EXPECT_FALSE(config.contains(Position{0, -101}));
}

} // namespace
} // namespace medsync
