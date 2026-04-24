#include "Log/Log.h"
#include <gtest/gtest.h>

struct TestObj
{
    Log::Logger _logger;
    explicit TestObj(int id)
        : _logger(std::format("Test/{}", id))
    {}

    void Foo()
    {
        _logger.Trace("trace");
        _logger.Debug("debug");
        _logger.Info("info");
        _logger.Warn("warn");
        _logger.Error("error");
        _logger.Fatal("fatal");
    }
};

TEST(LoggerTest, UsingLogger)
{
    auto obj1 = TestObj{1};
    auto obj2 = TestObj{2};

    obj1.Foo();
    obj2.Foo();

    EXPECT_TRUE(true);
}
