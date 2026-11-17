#include "medsync/SensorMonitor.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <string>

#include "medsync/Config.hpp"
#include "medsync/Logger.hpp"

namespace medsync {
namespace {

using namespace std::chrono_literals;

class SensorMonitorTest : public ::testing::Test {
protected:
    Config config_;
    std::ostringstream sink_;
    Logger logger_{sink_};
    SensorMonitor monitor_{config_, logger_};
};

TEST_F(SensorMonitorTest, LatestStartsEmpty) {
    const SensorReading reading = monitor_.latest();
    EXPECT_EQ(reading.sequence, 0u);
    EXPECT_DOUBLE_EQ(reading.temperatureC, 0.0);
    EXPECT_DOUBLE_EQ(reading.forceN, 0.0);
}

TEST_F(SensorMonitorTest, SampleOnceIsBoundedAndAdvancesSequence) {
    const SensorReading first = monitor_.sampleOnce();
    const SensorReading second = monitor_.sampleOnce();

    EXPECT_EQ(first.sequence, 1u);
    EXPECT_EQ(second.sequence, 2u);
    EXPECT_GE(first.temperatureC, 33.0);
    EXPECT_LE(first.temperatureC, 38.0);
    EXPECT_GE(first.forceN, 0.0);
    EXPECT_LE(first.forceN, 12.0);
    EXPECT_EQ(monitor_.latest().sequence, 2u);
}

TEST_F(SensorMonitorTest, TwoInstancesProduceTheSameFirstReading) {
    SensorMonitor other(config_, logger_);

    const SensorReading first = monitor_.sampleOnce();
    const SensorReading second = other.sampleOnce();

    EXPECT_EQ(first.sequence, second.sequence);
    EXPECT_DOUBLE_EQ(first.temperatureC, second.temperatureC);
    EXPECT_DOUBLE_EQ(first.forceN, second.forceN);
}

TEST_F(SensorMonitorTest, PeriodicWorkerPublishesReadings) {
    config_.sensorPeriod = 5ms;
    SensorMonitor monitor(config_, logger_);

    monitor.start();
    ASSERT_TRUE(monitor.waitForReadingAfter(0, 2000ms));
    const SensorReading reading = monitor.latest();
    EXPECT_GT(reading.sequence, 0u);
    EXPECT_GE(reading.temperatureC, 33.0);
    EXPECT_LE(reading.temperatureC, 38.0);

    monitor.shutdown();
    monitor.shutdown();
}

TEST_F(SensorMonitorTest, ShutdownDoesNotWaitForTheNextPeriod) {
    config_.sensorPeriod = 5000ms;
    SensorMonitor monitor(config_, logger_);

    monitor.start();
    ASSERT_TRUE(monitor.waitForReadingAfter(0, 2000ms));

    const auto started = std::chrono::steady_clock::now();
    monitor.shutdown();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_LT(elapsed, 2000ms);
}

TEST_F(SensorMonitorTest, WaitForReadingTimesOutWithoutAWorker) {
    EXPECT_FALSE(monitor_.waitForReadingAfter(0, 20ms));
}

} // namespace
} // namespace medsync
