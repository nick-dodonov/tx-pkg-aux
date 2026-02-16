#include "Log/Log.h"
#include "Boot/Boot.h"
#include <gtest/gtest.h>

static bool _fail = false;

TEST(LogTest, Works) {
    Log::Info("Test info message");
    Log::Warn("Test warning message");
    Log::Error("Test error message");
    EXPECT_TRUE(!_fail);
}

int main(int argc, char** argv)
{
    if (argc > 1) {
        _fail = true;
    }
    Boot::DefaultInit(argc, const_cast<const char**>(argv));
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
