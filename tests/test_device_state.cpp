#include "medsync/DeviceState.hpp"

#include <gtest/gtest.h>

#include <string>

namespace medsync {
namespace {

TEST(DeviceStateTest, NormalWorkflowTransitionsAreAllowed) {
    EXPECT_TRUE(canTransition(DeviceState::Idle, DeviceState::Homing));
    EXPECT_TRUE(canTransition(DeviceState::Homing, DeviceState::Ready));
    EXPECT_TRUE(canTransition(DeviceState::Ready, DeviceState::Positioning));
    EXPECT_TRUE(canTransition(DeviceState::Positioning, DeviceState::Scanning));
    EXPECT_TRUE(canTransition(DeviceState::Scanning, DeviceState::Ready));
    EXPECT_TRUE(canTransition(DeviceState::Fault, DeviceState::Idle));
    EXPECT_TRUE(canTransition(DeviceState::EmergencyStop, DeviceState::Idle));
}

TEST(DeviceStateTest, IllegalTransitionsAreRejected) {
    EXPECT_FALSE(canTransition(DeviceState::Idle, DeviceState::Ready));
    EXPECT_FALSE(canTransition(DeviceState::Idle, DeviceState::Scanning));
    EXPECT_FALSE(canTransition(DeviceState::Ready, DeviceState::Homing));
    EXPECT_FALSE(canTransition(DeviceState::Scanning, DeviceState::Positioning));
    EXPECT_FALSE(canTransition(DeviceState::Homing, DeviceState::Scanning));
}

TEST(DeviceStateTest, SelfTransitionsAreRejected) {
    EXPECT_FALSE(canTransition(DeviceState::Idle, DeviceState::Idle));
    EXPECT_FALSE(canTransition(DeviceState::Ready, DeviceState::Ready));
    EXPECT_FALSE(canTransition(DeviceState::EmergencyStop,
                               DeviceState::EmergencyStop));
}

TEST(DeviceStateTest, EmergencyStopIsAllowedFromEveryActiveState) {
    const DeviceState activeStates[] = {
        DeviceState::Idle,       DeviceState::Homing,
        DeviceState::Ready,      DeviceState::Positioning,
        DeviceState::Scanning,   DeviceState::Fault,
    };

    for (const DeviceState state : activeStates) {
        EXPECT_TRUE(canTransition(state, DeviceState::EmergencyStop))
            << "from " << to_string(state);
    }
}

TEST(DeviceStateTest, ShuttingDownIsTerminal) {
    const DeviceState allStates[] = {
        DeviceState::Idle,        DeviceState::Homing,
        DeviceState::Ready,       DeviceState::Positioning,
        DeviceState::Scanning,    DeviceState::Fault,
        DeviceState::EmergencyStop,
    };

    for (const DeviceState state : allStates) {
        EXPECT_FALSE(canTransition(DeviceState::ShuttingDown, state));
    }
}

TEST(DeviceStateTest, ShutdownIsAllowedFromEveryNonTerminalState) {
    const DeviceState states[] = {
        DeviceState::Idle,        DeviceState::Homing,
        DeviceState::Ready,       DeviceState::Positioning,
        DeviceState::Scanning,    DeviceState::Fault,
        DeviceState::EmergencyStop,
    };

    for (const DeviceState state : states) {
        EXPECT_TRUE(canTransition(state, DeviceState::ShuttingDown))
            << "from " << to_string(state);
    }
}

TEST(DeviceStateTest, StateNamesAreStable) {
    EXPECT_EQ(std::string(to_string(DeviceState::Idle)), "Idle");
    EXPECT_EQ(std::string(to_string(DeviceState::Homing)), "Homing");
    EXPECT_EQ(std::string(to_string(DeviceState::Ready)), "Ready");
    EXPECT_EQ(std::string(to_string(DeviceState::Positioning)), "Positioning");
    EXPECT_EQ(std::string(to_string(DeviceState::Scanning)), "Scanning");
    EXPECT_EQ(std::string(to_string(DeviceState::Fault)), "Fault");
    EXPECT_EQ(std::string(to_string(DeviceState::EmergencyStop)),
              "EmergencyStop");
    EXPECT_EQ(std::string(to_string(DeviceState::ShuttingDown)),
              "ShuttingDown");
}

} // namespace
} // namespace medsync
