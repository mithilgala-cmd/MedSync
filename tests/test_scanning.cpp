#include "medsync/ScanningModule.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include "medsync/BoundedQueue.hpp"
#include "medsync/Config.hpp"
#include "medsync/DeviceEvent.hpp"
#include "medsync/Logger.hpp"

namespace medsync {
namespace {

class ScanningModuleTest : public ::testing::Test {
protected:
    Config config_ = [] {
        Config config;
        config.scanDuration = std::chrono::milliseconds(0);
        return config;
    }();
    std::ostringstream sink_;
    Logger logger_{sink_};
    ScanningModule scanner_{config_, logger_};
};

TEST_F(ScanningModuleTest, ProducesResultForValidSample) {
    const Sample sample{"S-001", Position{10, 20}, SampleStatus::Received};

    const ScanResult result = scanner_.scan(sample);

    EXPECT_TRUE(result.successful);
    EXPECT_EQ(result.sampleId, "S-001");
    EXPECT_GE(result.qualityScore, 80);
    EXPECT_LE(result.qualityScore, 100);
    EXPECT_FALSE(result.note.empty());
}

TEST_F(ScanningModuleTest, IsDeterministicForTheSameSampleId) {
    const Sample sample{"S-042", Position{0, 0}, SampleStatus::Received};

    const ScanResult first = scanner_.scan(sample);
    const ScanResult second = scanner_.scan(sample);

    EXPECT_EQ(first.qualityScore, second.qualityScore);
}

TEST_F(ScanningModuleTest, RejectsEmptySampleId) {
    const Sample sample{"", Position{0, 0}, SampleStatus::Received};

    const ScanResult result = scanner_.scan(sample);

    EXPECT_FALSE(result.successful);
    EXPECT_EQ(result.qualityScore, 0);
}

TEST(ScanningWorkerTest, ReportsScanCompletion) {
    Config config;
    config.scanDuration = std::chrono::milliseconds(0);
    std::ostringstream sink;
    Logger logger(sink);
    BoundedQueue<DeviceEvent> events(8);
    ScanningModule scanner(config, logger);

    scanner.start(events);
    const Sample sample{"S-101", Position{10, 20}, SampleStatus::Positioned};
    std::string error;
    ASSERT_TRUE(scanner.submitScan(sample, 42, error)) << error;

    DeviceEvent event;
    ASSERT_TRUE(events.popFor(event, std::chrono::milliseconds(2000)));
    EXPECT_EQ(event.type, DeviceEventType::ScanCompleted);
    EXPECT_EQ(event.cycleId, 42u);
    EXPECT_TRUE(event.scan.successful);
    EXPECT_EQ(event.scan.sampleId, "S-101");

    scanner.shutdown();
    EXPECT_TRUE(scanner.waitUntilIdle(std::chrono::milliseconds(100)));
}

TEST(ScanningWorkerTest, RejectsSubmissionWithoutWorker) {
    Config config;
    std::ostringstream sink;
    Logger logger(sink);
    ScanningModule scanner(config, logger);
    const Sample sample{"S-101", Position{0, 0}, SampleStatus::Received};

    std::string error;
    EXPECT_FALSE(scanner.submitScan(sample, 1, error));
    EXPECT_NE(error.find("not running"), std::string::npos);
}

TEST(ScanningWorkerTest, CancelInterruptsTheSimulatedScanDelay) {
    Config config;
    config.scanDuration = std::chrono::milliseconds(500);
    std::ostringstream sink;
    Logger logger(sink);
    BoundedQueue<DeviceEvent> events(8);
    ScanningModule scanner(config, logger);

    scanner.start(events);
    const Sample sample{"S-202", Position{10, 10}, SampleStatus::Positioned};
    std::string error;
    ASSERT_TRUE(scanner.submitScan(sample, 9, error)) << error;

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    scanner.cancelPending();

    DeviceEvent event;
    ASSERT_TRUE(events.popFor(event, std::chrono::milliseconds(2000)));
    EXPECT_EQ(event.type, DeviceEventType::ScanFailed);
    EXPECT_EQ(event.cycleId, 9u);
    EXPECT_EQ(event.scan.note, "scan aborted");
    EXPECT_TRUE(scanner.waitUntilIdle(std::chrono::milliseconds(1000)));

    scanner.shutdown();
}

} // namespace
} // namespace medsync
