#include "medsync/DeviceController.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "medsync/Logger.hpp"

namespace medsync {
namespace {

using namespace std::chrono_literals;

Config fastConfig() {
    Config config;
    config.stepDelay = 0ms;
    config.scanDuration = 0ms;
    return config;
}

TEST(DeviceConcurrencyTest, FiftySequentialCyclesComplete) {
    Config config = fastConfig();
    std::ostringstream sink;
    Logger logger(sink);
    DeviceController controller(config, logger);

    std::string error;
    ASSERT_TRUE(controller.initialise(error)) << error;
    ASSERT_TRUE(controller.home(error)) << error;

    for (int i = 0; i < 50; ++i) {
        const std::string id = "S-" + std::to_string(100 + i);
        const Position target{i % 40, (i * 2) % 40};

        ASSERT_TRUE(controller.loadSample(id, target, error)) << error;
        ASSERT_TRUE(controller.runCycle(error)) << error;
        ASSERT_TRUE(controller.waitForCycleCompletion(2000ms))
            << "cycle " << i << " did not complete";
        ASSERT_EQ(controller.state(), DeviceState::Ready);
        ASSERT_FALSE(controller.hasActiveSample());
        ASSERT_EQ(controller.position(), target);
    }

    ASSERT_TRUE(controller.shutdown(error)) << error;
}

TEST(DeviceConcurrencyTest, EmergencyStopDuringProcessingCanRecover) {
    Config config = fastConfig();
    config.stepDelay = 2ms;
    config.scanDuration = 10ms;
    std::ostringstream sink;
    Logger logger(sink);
    DeviceController controller(config, logger);

    std::string error;
    ASSERT_TRUE(controller.initialise(error)) << error;
    ASSERT_TRUE(controller.home(error)) << error;
    ASSERT_TRUE(controller.loadSample("S-001", Position{40, 40}, error)) << error;
    ASSERT_TRUE(controller.runCycle(error)) << error;

    // The stop may land before or after the worker finishes; both are valid.
    ASSERT_TRUE(controller.emergencyStop(error)) << error;
    EXPECT_EQ(controller.state(), DeviceState::EmergencyStop);
    EXPECT_FALSE(controller.hasActiveSample());

    ASSERT_TRUE(controller.releaseEmergencyStop(error)) << error;
    ASSERT_TRUE(controller.reset(error)) << error;
    ASSERT_TRUE(controller.home(error)) << error;

    ASSERT_TRUE(controller.loadSample("S-002", Position{-20, 15}, error))
        << error;
    ASSERT_TRUE(controller.runCycle(error)) << error;
    ASSERT_TRUE(controller.waitForCycleCompletion(2000ms));
    EXPECT_EQ(controller.state(), DeviceState::Ready);
    EXPECT_EQ(controller.position(), (Position{-20, 15}));
    EXPECT_FALSE(controller.hasActiveSample());

    ASSERT_TRUE(controller.shutdown(error)) << error;
}

TEST(DeviceConcurrencyTest, ShutdownDuringProcessingJoinsCleanly) {
    Config config = fastConfig();
    config.stepDelay = 5ms;
    config.scanDuration = 20ms;
    std::ostringstream sink;
    Logger logger(sink);
    DeviceController controller(config, logger);

    std::string error;
    ASSERT_TRUE(controller.initialise(error)) << error;
    ASSERT_TRUE(controller.home(error)) << error;
    ASSERT_TRUE(controller.loadSample("S-001", Position{90, -90}, error))
        << error;
    ASSERT_TRUE(controller.runCycle(error)) << error;

    ASSERT_TRUE(controller.shutdown(error)) << error;
    EXPECT_EQ(controller.state(), DeviceState::ShuttingDown);
    EXPECT_TRUE(controller.shutdown(error));
}

TEST(DeviceConcurrencyTest, StatusQueriesRaceWithCycleExecution) {
    Config config = fastConfig();
    std::ostringstream sink;
    Logger logger(sink);
    DeviceController controller(config, logger);

    std::string error;
    ASSERT_TRUE(controller.initialise(error)) << error;
    ASSERT_TRUE(controller.home(error)) << error;

    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&controller, &stop] {
            while (!stop.load(std::memory_order_acquire)) {
                (void)controller.statusText();
                (void)controller.state();
                (void)controller.activeSample();
                std::this_thread::yield();
            }
        });
    }

    for (int i = 0; i < 10; ++i) {
        const std::string id = "R-" + std::to_string(i);
        if (!controller.loadSample(id, Position{i, i}, error) ||
            !controller.runCycle(error) ||
            !controller.waitForCycleCompletion(2000ms)) {
            ADD_FAILURE() << "cycle " << i << " failed: " << error;
            break;
        }
    }

    stop.store(true, std::memory_order_release);
    for (std::thread& reader : readers) {
        reader.join();
    }

    ASSERT_TRUE(controller.shutdown(error)) << error;
}

TEST(DeviceConcurrencyTest, DestructorShutsDownRunningThreads) {
    Config config = fastConfig();
    config.stepDelay = 5ms;
    std::ostringstream sink;
    Logger logger(sink);

    {
        DeviceController controller(config, logger);
        std::string error;
        ASSERT_TRUE(controller.initialise(error)) << error;
        ASSERT_TRUE(controller.home(error)) << error;
        ASSERT_TRUE(controller.loadSample("S-001", Position{30, 30}, error))
            << error;
        ASSERT_TRUE(controller.runCycle(error)) << error;
        // No explicit shutdown: the destructor must abort and join.
    }

    SUCCEED();
}

} // namespace
} // namespace medsync
