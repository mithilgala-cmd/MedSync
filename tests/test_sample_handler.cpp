#include "medsync/SampleHandler.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "medsync/Logger.hpp"

namespace medsync {
namespace {

class SampleHandlerTest : public ::testing::Test {
protected:
    Sample sample() const {
        return Sample{"S-001", Position{10, 20}, SampleStatus::Received};
    }

    Config config_;
    std::ostringstream sink_;
    Logger logger_{sink_};
    SampleHandler handler_{config_, logger_};
};

TEST_F(SampleHandlerTest, AcceptsValidSample) {
    std::string error;
    ASSERT_TRUE(handler_.accept(sample(), error)) << error;

    EXPECT_TRUE(handler_.hasActive());
    ASSERT_TRUE(handler_.active().has_value());
    EXPECT_EQ(handler_.active()->id, "S-001");
    EXPECT_EQ(handler_.active()->status, SampleStatus::Received);
}

TEST_F(SampleHandlerTest, RejectsSecondSampleWhileActive) {
    std::string error;
    ASSERT_TRUE(handler_.accept(sample(), error)) << error;

    EXPECT_FALSE(handler_.accept(sample(), error));
    EXPECT_NE(error.find("already loaded"), std::string::npos);
}

TEST_F(SampleHandlerTest, RejectsInvalidIds) {
    std::string error;

    Sample emptyId = sample();
    emptyId.id.clear();
    EXPECT_FALSE(handler_.accept(emptyId, error));
    EXPECT_NE(error.find("empty"), std::string::npos);

    Sample spaceInId = sample();
    spaceInId.id = "S 001";
    EXPECT_FALSE(handler_.accept(spaceInId, error));
    EXPECT_NE(error.find("letters, digits"), std::string::npos);

    Sample underscore = sample();
    underscore.id = "S_001";
    EXPECT_FALSE(handler_.accept(underscore, error));

    Sample tooLong = sample();
    tooLong.id.assign(config_.maxSampleIdLength + 1, 'A');
    EXPECT_FALSE(handler_.accept(tooLong, error));
    EXPECT_NE(error.find("exceeds"), std::string::npos);
}

TEST_F(SampleHandlerTest, RejectsTargetOutsideBounds) {
    Sample outOfBounds = sample();
    outOfBounds.target = Position{0, 101};

    std::string error;
    EXPECT_FALSE(handler_.accept(outOfBounds, error));
    EXPECT_NE(error.find("outside"), std::string::npos);
}

TEST_F(SampleHandlerTest, TracksStatusTransitions) {
    std::string error;
    ASSERT_TRUE(handler_.accept(sample(), error)) << error;

    handler_.markPositioned();
    EXPECT_EQ(handler_.active()->status, SampleStatus::Positioned);

    handler_.markScanning();
    EXPECT_EQ(handler_.active()->status, SampleStatus::Scanning);

    ScanResult result;
    result.sampleId = "S-001";
    result.successful = true;
    result.qualityScore = 90;
    handler_.complete(result);

    EXPECT_FALSE(handler_.hasActive());
    EXPECT_NE(sink_.str().find("completed"), std::string::npos);
}

TEST_F(SampleHandlerTest, FailClearsActiveSample) {
    std::string error;
    ASSERT_TRUE(handler_.accept(sample(), error)) << error;

    handler_.fail("test fault");

    EXPECT_FALSE(handler_.hasActive());
    EXPECT_NE(sink_.str().find("failed: test fault"), std::string::npos);
}

TEST_F(SampleHandlerTest, FailWithoutActiveSampleIsSafe) {
    EXPECT_NO_THROW(handler_.fail("no sample"));
    EXPECT_FALSE(handler_.hasActive());
}

} // namespace
} // namespace medsync
