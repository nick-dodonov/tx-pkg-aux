#include "Log/Log.h"
#include <gtest/gtest.h>

TEST(LogTest, InfoWorks) {
    Log::Info("Test info message");
    EXPECT_TRUE(true);
}

TEST(LogTest, WarnWorks) {
    Log::Warn("Test warning message");
    EXPECT_TRUE(true);
}

TEST(LogTest, ErrorWorks) {
    Log::Error("Test error message");
    EXPECT_TRUE(true);
}
