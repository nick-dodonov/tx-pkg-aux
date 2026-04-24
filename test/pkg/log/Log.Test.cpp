#include "Log/Log.h"
#include <gtest/gtest.h>


TEST(LogTest, AllMsg)
{
    Log::Trace("Integration test trace");
    Log::Debug("Integration test debug");
    Log::Info("Integration test info");
    Log::Warn("Integration test warn");
    Log::Error("Integration test error");
    Log::Fatal("Integration test fatal");

    // If we reached this line, all methods executed without crashes
    EXPECT_TRUE(true);
}
