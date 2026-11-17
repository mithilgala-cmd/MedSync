#include "medsync/Logger.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace medsync {
namespace {

TEST(LoggerTest, WritesMessagesInOrderAndCountsThem) {
    std::ostringstream sink;
    Logger logger(sink);

    logger.info("first event");
    logger.warning("second event");

    const std::string text = sink.str();
    const auto first = text.find("first event");
    const auto second = text.find("second event");

    ASSERT_NE(first, std::string::npos);
    ASSERT_NE(second, std::string::npos);
    EXPECT_LT(first, second);
    EXPECT_EQ(logger.entryCount(), 2u);
}

TEST(LoggerTest, IncludesLevelTags) {
    std::ostringstream sink;
    Logger logger(sink);

    logger.debug("debug message");
    logger.info("info message");
    logger.warning("warning message");
    logger.error("error message");

    const std::string text = sink.str();
    EXPECT_NE(text.find("[DEBUG]"), std::string::npos);
    EXPECT_NE(text.find("[INFO]"), std::string::npos);
    EXPECT_NE(text.find("[WARN]"), std::string::npos);
    EXPECT_NE(text.find("[ERROR]"), std::string::npos);
    EXPECT_EQ(logger.entryCount(), 4u);
}

TEST(LoggerTest, StartsEachEntryOnItsOwnLine) {
    std::ostringstream sink;
    Logger logger(sink);

    logger.info("alpha");
    logger.info("beta");

    const std::string text = sink.str();
    EXPECT_NE(text.find("alpha\n"), std::string::npos);
    EXPECT_NE(text.find("beta\n"), std::string::npos);
}

} // namespace
} // namespace medsync
