#include "medsync/PositioningController.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include "medsync/BoundedQueue.hpp"
#include "medsync/DeviceEvent.hpp"
#include "medsync/Logger.hpp"

namespace medsync {
namespace {

class PositioningControllerTest : public ::testing::Test {
protected:
    Config config_ = [] {
        Config config;
        config.stepDelay = std::chrono::milliseconds(0);
        return config;
    }();
    std::ostringstream sink_;
    Logger logger_{sink_};
    PositioningController stage_{config_, logger_};
};

TEST_F(PositioningControllerTest, StartsUnhomed) {
    EXPECT_FALSE(stage_.isHomed());
}

TEST_F(PositioningControllerTest, HomeSetsPositionAndHomedFlag) {
    config_.homePosition = Position{5, -5};
    PositioningController stage{config_, logger_};

    std::string error;
    ASSERT_TRUE(stage.home(error)) << error;
    EXPECT_TRUE(stage.isHomed());
    EXPECT_EQ(stage.position(), (Position{5, -5}));
}

TEST_F(PositioningControllerTest, MoveRequiresHomingFirst) {
    std::string error;
    EXPECT_FALSE(stage_.moveTo(Position{10, 10}, error));
    EXPECT_NE(error.find("homed"), std::string::npos);
}

TEST_F(PositioningControllerTest, MoveReachesTargetExactly) {
    std::string error;
    ASSERT_TRUE(stage_.home(error)) << error;
    ASSERT_TRUE(stage_.moveTo(Position{25, -35}, error)) << error;

    EXPECT_EQ(stage_.position(), (Position{25, -35}));
}

TEST_F(PositioningControllerTest, MoveHandlesDistanceSmallerThanOneStep) {
    std::string error;
    ASSERT_TRUE(stage_.home(error)) << error;
    ASSERT_TRUE(stage_.moveTo(Position{7, -3}, error)) << error;

    EXPECT_EQ(stage_.position(), (Position{7, -3}));
}

TEST_F(PositioningControllerTest, RejectsTargetOutsideBounds) {
    std::string error;
    ASSERT_TRUE(stage_.home(error)) << error;

    EXPECT_FALSE(stage_.moveTo(Position{101, 0}, error));
    EXPECT_NE(error.find("outside"), std::string::npos);
    EXPECT_EQ(stage_.position(), config_.homePosition);
}

TEST_F(PositioningControllerTest, InvalidatedHomingBlocksMovement) {
    std::string error;
    ASSERT_TRUE(stage_.home(error)) << error;
    stage_.invalidateHoming();

    EXPECT_FALSE(stage_.isHomed());
    EXPECT_FALSE(stage_.moveTo(Position{10, 10}, error));
}

TEST_F(PositioningControllerTest, HomingReturnsStageToHomePosition) {
    std::string error;
    ASSERT_TRUE(stage_.home(error)) << error;
    ASSERT_TRUE(stage_.moveTo(Position{40, 40}, error)) << error;
    ASSERT_TRUE(stage_.home(error)) << error;

    EXPECT_EQ(stage_.position(), config_.homePosition);
}

TEST(PositioningWorkerTest, ReportsMoveCompletion) {
    Config config;
    config.stepDelay = std::chrono::milliseconds(0);
    std::ostringstream sink;
    Logger logger(sink);
    BoundedQueue<DeviceEvent> events(8);
    PositioningController stage(config, logger);

    stage.start(events);
    std::string error;
    ASSERT_TRUE(stage.home(error)) << error;
    ASSERT_TRUE(stage.submitMove(Position{20, 30}, 7, error)) << error;

    DeviceEvent event;
    ASSERT_TRUE(events.popFor(event, std::chrono::milliseconds(2000)));
    EXPECT_EQ(event.type, DeviceEventType::MoveCompleted);
    EXPECT_EQ(event.cycleId, 7u);
    EXPECT_EQ(stage.position(), (Position{20, 30}));

    stage.shutdown();
    EXPECT_TRUE(stage.waitUntilIdle(std::chrono::milliseconds(100)));
}

TEST(PositioningWorkerTest, RejectsSubmissionWithoutWorker) {
    Config config;
    std::ostringstream sink;
    Logger logger(sink);
    PositioningController stage(config, logger);

    std::string error;
    ASSERT_TRUE(stage.home(error)) << error;

    EXPECT_FALSE(stage.submitMove(Position{10, 10}, 1, error));
    EXPECT_NE(error.find("not running"), std::string::npos);
}

TEST(PositioningWorkerTest, ShutdownIsIdempotent) {
    Config config;
    std::ostringstream sink;
    Logger logger(sink);
    BoundedQueue<DeviceEvent> events(8);
    PositioningController stage(config, logger);

    stage.start(events);
    stage.shutdown();
    stage.shutdown();

    SUCCEED();
}

TEST(PositioningWorkerTest, CancelInterruptsTheStepDelay) {
    Config config;
    config.stepDelay = std::chrono::milliseconds(20);
    std::ostringstream sink;
    Logger logger(sink);
    BoundedQueue<DeviceEvent> events(8);
    PositioningController stage(config, logger);

    stage.start(events);
    std::string error;
    ASSERT_TRUE(stage.home(error)) << error;
    ASSERT_TRUE(stage.submitMove(Position{100, 100}, 11, error)) << error;

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    stage.cancelPending();

    DeviceEvent event;
    ASSERT_TRUE(events.popFor(event, std::chrono::milliseconds(2000)));
    EXPECT_TRUE(event.type == DeviceEventType::MoveFailed ||
                event.type == DeviceEventType::MoveAborted);
    EXPECT_EQ(event.cycleId, 11u);
    EXPECT_NE(stage.position(), (Position{100, 100}));
    EXPECT_TRUE(stage.waitUntilIdle(std::chrono::milliseconds(1000)));

    stage.shutdown();
}

} // namespace
} // namespace medsync
