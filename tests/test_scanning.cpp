#include "medsync/ScanningModule.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "medsync/Logger.hpp"

namespace medsync {
namespace {

class ScanningModuleTest : public ::testing::Test {
protected:
    std::ostringstream sink_;
    Logger logger_{sink_};
    ScanningModule scanner_{logger_};
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

} // namespace
} // namespace medsync
