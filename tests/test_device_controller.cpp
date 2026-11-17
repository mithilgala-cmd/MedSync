#include "medsync/DeviceController.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "medsync/Logger.hpp"

namespace medsync {
namespace {

class DeviceControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string error;
        ASSERT_TRUE(controller_.initialise(error)) << error;
    }

    void homeDevice() {
        std::string error;
        ASSERT_TRUE(controller_.home(error)) << error;
        ASSERT_EQ(controller_.state(), DeviceState::Ready);
    }

    Config config_;
    std::ostringstream sink_;
    Logger logger_{sink_};
    DeviceController controller_{config_, logger_};
};

TEST_F(DeviceControllerTest, CannotLoadBeforeInitialiseOrHome) {
    Config config;
    std::ostringstream sink;
    Logger logger(sink);
    DeviceController fresh(config, logger);

    std::string error;
    EXPECT_FALSE(fresh.loadSample("S-001", Position{0, 0}, error));
    EXPECT_NE(error.find("not initialised"), std::string::npos);

    ASSERT_TRUE(fresh.initialise(error)) << error;
    EXPECT_FALSE(fresh.loadSample("S-001", Position{0, 0}, error));
    EXPECT_NE(error.find("Ready"), std::string::npos);
}

TEST_F(DeviceControllerTest, InitialiseRejectsInvalidConfig) {
    Config invalid;
    invalid.stepSize = 0;
    std::ostringstream sink;
    Logger logger(sink);
    DeviceController controller(invalid, logger);

    std::string error;
    EXPECT_FALSE(controller.initialise(error));
    EXPECT_FALSE(error.empty());
}

TEST_F(DeviceControllerTest, InitialiseIsIdempotentOnlyOnce) {
    std::string error;
    EXPECT_FALSE(controller_.initialise(error));
    EXPECT_NE(error.find("already"), std::string::npos);
}

TEST_F(DeviceControllerTest, HomeMovesFromIdleToReady) {
    EXPECT_EQ(controller_.state(), DeviceState::Idle);

    homeDevice();

    EXPECT_TRUE(controller_.positioning().isHomed());
    EXPECT_EQ(controller_.positioning().position(), config_.homePosition);
}

TEST_F(DeviceControllerTest, HappyPathCompletesSampleAndReturnsToReady) {
    homeDevice();

    std::string error;
    ASSERT_TRUE(controller_.loadSample("S-001", Position{20, 30}, error))
        << error;
    EXPECT_TRUE(controller_.samples().hasActive());
    EXPECT_EQ(controller_.samples().active()->status, SampleStatus::Received);

    ASSERT_TRUE(controller_.runCycle(error)) << error;

    EXPECT_EQ(controller_.state(), DeviceState::Ready);
    EXPECT_FALSE(controller_.samples().hasActive());
    EXPECT_EQ(controller_.positioning().position(), (Position{20, 30}));
    EXPECT_NE(sink_.str().find("sample 'S-001' completed"), std::string::npos);
}

TEST_F(DeviceControllerTest, RejectsSecondSampleWhileOneIsLoaded) {
    homeDevice();

    std::string error;
    ASSERT_TRUE(controller_.loadSample("S-001", Position{10, 10}, error))
        << error;

    EXPECT_FALSE(controller_.loadSample("S-002", Position{20, 20}, error));
    EXPECT_NE(error.find("already loaded"), std::string::npos);
}

TEST_F(DeviceControllerTest, RejectsSampleOutsideStageBounds) {
    homeDevice();

    std::string error;
    EXPECT_FALSE(controller_.loadSample("S-001", Position{101, 0}, error));
    EXPECT_NE(error.find("outside"), std::string::npos);
}

TEST_F(DeviceControllerTest, RunWithoutSampleIsRejected) {
    homeDevice();

    std::string error;
    EXPECT_FALSE(controller_.runCycle(error));
    EXPECT_NE(error.find("no sample"), std::string::npos);
}

TEST_F(DeviceControllerTest, EmergencyStopLatchesAndFailsActiveSample) {
    homeDevice();

    std::string error;
    ASSERT_TRUE(controller_.loadSample("S-001", Position{10, 10}, error))
        << error;

    ASSERT_TRUE(controller_.emergencyStop(error)) << error;
    EXPECT_EQ(controller_.state(), DeviceState::EmergencyStop);
    EXPECT_FALSE(controller_.samples().hasActive());
    EXPECT_FALSE(controller_.positioning().isHomed());
    EXPECT_NE(sink_.str().find("failed: emergency stop"), std::string::npos);
}

TEST_F(DeviceControllerTest, EmergencyStopIsIdempotent) {
    homeDevice();

    std::string error;
    ASSERT_TRUE(controller_.emergencyStop(error));
    EXPECT_TRUE(controller_.emergencyStop(error));
    EXPECT_EQ(controller_.state(), DeviceState::EmergencyStop);
}

TEST_F(DeviceControllerTest, ResetRequiresEmergencyStopToBeReleased) {
    homeDevice();

    std::string error;
    ASSERT_TRUE(controller_.emergencyStop(error));

    EXPECT_FALSE(controller_.reset(error));
    EXPECT_NE(error.find("latched"), std::string::npos);

    ASSERT_TRUE(controller_.releaseEmergencyStop(error)) << error;
    ASSERT_TRUE(controller_.reset(error)) << error;

    EXPECT_EQ(controller_.state(), DeviceState::Idle);
    EXPECT_FALSE(controller_.positioning().isHomed());
}

TEST_F(DeviceControllerTest, ResetIsRejectedOutsideSafetyStates) {
    homeDevice();

    std::string error;
    EXPECT_FALSE(controller_.reset(error));
    EXPECT_NE(error.find("Fault or EmergencyStop"), std::string::npos);
}

TEST_F(DeviceControllerTest, ReleaseWithoutEmergencyStopIsRejected) {
    homeDevice();

    std::string error;
    EXPECT_FALSE(controller_.releaseEmergencyStop(error));
    EXPECT_NE(error.find("no emergency stop"), std::string::npos);
}

TEST_F(DeviceControllerTest, CanHomeAgainAfterReset) {
    homeDevice();

    std::string error;
    ASSERT_TRUE(controller_.emergencyStop(error));
    ASSERT_TRUE(controller_.releaseEmergencyStop(error));
    ASSERT_TRUE(controller_.reset(error));
    ASSERT_TRUE(controller_.home(error)) << error;

    EXPECT_EQ(controller_.state(), DeviceState::Ready);
    EXPECT_TRUE(controller_.positioning().isHomed());
}

TEST_F(DeviceControllerTest, ShutdownIsIdempotentAndTerminal) {
    homeDevice();

    std::string error;
    ASSERT_TRUE(controller_.shutdown(error));
    EXPECT_EQ(controller_.state(), DeviceState::ShuttingDown);
    EXPECT_TRUE(controller_.shutdown(error));

    EXPECT_FALSE(controller_.home(error));
}

TEST_F(DeviceControllerTest, StatusTextIncludesStateAndSample) {
    homeDevice();
    std::string error;
    ASSERT_TRUE(controller_.loadSample("S-777", Position{5, 5}, error))
        << error;

    const std::string status = controller_.statusText();
    EXPECT_NE(status.find("state=Ready"), std::string::npos);
    EXPECT_NE(status.find("S-777"), std::string::npos);
    EXPECT_NE(status.find("Received"), std::string::npos);
}

} // namespace
} // namespace medsync
