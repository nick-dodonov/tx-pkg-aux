#include "Log/Exception.h"
#include "Log/Log.h"
#include <stdexcept>
#include <gtest/gtest.h>

TEST(ExceptionMessageTest, NullPtr) {
    EXPECT_EQ(Log::ExceptionMessage(nullptr), "");
}

TEST(ExceptionMessageTest, StdException) {
    const auto ep = std::make_exception_ptr(std::runtime_error("oops"));
    EXPECT_EQ(Log::ExceptionMessage(ep), "oops");
}

TEST(ExceptionMessageTest, UnknownException) {
    const auto ep = std::make_exception_ptr(42);
    EXPECT_EQ(Log::ExceptionMessage(ep), "<unknown exception>");
}

TEST(ExceptionMessageTest, UsableInLogCall) {
    const auto ep = std::make_exception_ptr(std::runtime_error("something failed"));
    Log::Error("operation failed: {}", Log::ExceptionMessage(ep));
    EXPECT_TRUE(true);
}
