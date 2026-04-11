#include "Log/Sep.h"
#include <cstdint>
#include <format>
#include <limits>
#include <gtest/gtest.h>

TEST(SepTest, Zero) {
    EXPECT_EQ(std::format("{}", Log::Sep{0}), "0");
}

TEST(SepTest, SmallPositive) {
    EXPECT_EQ(std::format("{}", Log::Sep{1}), "1");
    EXPECT_EQ(std::format("{}", Log::Sep{12}), "12");
    EXPECT_EQ(std::format("{}", Log::Sep{999}), "999");
}

TEST(SepTest, ThousandBoundary) {
    EXPECT_EQ(std::format("{}", Log::Sep{1000}), "1'000");
    EXPECT_EQ(std::format("{}", Log::Sep{1001}), "1'001");
    EXPECT_EQ(std::format("{}", Log::Sep{9999}), "9'999");
}

TEST(SepTest, LargePositive) {
    EXPECT_EQ(std::format("{}", Log::Sep{1'000'000}), "1'000'000");
    EXPECT_EQ(std::format("{}", Log::Sep{123'456'789}), "123'456'789");
}

TEST(SepTest, Negative) {
    EXPECT_EQ(std::format("{}", Log::Sep{-1}), "-1");
    EXPECT_EQ(std::format("{}", Log::Sep{-999}), "-999");
    EXPECT_EQ(std::format("{}", Log::Sep{-1000}), "-1'000");
    EXPECT_EQ(std::format("{}", Log::Sep{-2'286'995'118}), "-2'286'995'118");
}

TEST(SepTest, IntLimits) {
    EXPECT_EQ(std::format("{}", Log::Sep{std::numeric_limits<int32_t>::max()}), "2'147'483'647");
    EXPECT_EQ(std::format("{}", Log::Sep{std::numeric_limits<int32_t>::min()}), "-2'147'483'648");
}

TEST(SepTest, Int64Limits) {
    EXPECT_EQ(std::format("{}", Log::Sep{std::numeric_limits<int64_t>::max()}), "9'223'372'036'854'775'807");
    EXPECT_EQ(std::format("{}", Log::Sep{std::numeric_limits<int64_t>::min()}), "-9'223'372'036'854'775'808");
}

TEST(SepTest, Unsigned) {
    EXPECT_EQ(std::format("{}", Log::Sep{0u}), "0");
    EXPECT_EQ(std::format("{}", Log::Sep{42u}), "42");
    EXPECT_EQ(std::format("{}", Log::Sep{1'000u}), "1'000");
    EXPECT_EQ(std::format("{}", Log::Sep{std::numeric_limits<uint64_t>::max()}), "18'446'744'073'709'551'615");
}

TEST(SepTest, FormatSpecWidth) {
    EXPECT_EQ(std::format("{:>20}", Log::Sep{1'234}), "               1'234");
    EXPECT_EQ(std::format("{:<20}", Log::Sep{1'234}), "1'234               ");
}

TEST(SepTest, FormatSpecFill) {
    EXPECT_EQ(std::format("{:_>15}", Log::Sep{-42'000}), "________-42'000");
}

TEST(SepTest, CtadDeduction) {
    auto s = Log::Sep{42};
    static_assert(std::same_as<decltype(s), Log::Sep<int>>);

    auto u = Log::Sep{42u};
    static_assert(std::same_as<decltype(u), Log::Sep<unsigned int>>);

    auto l = Log::Sep{42L};
    static_assert(std::same_as<decltype(l), Log::Sep<long>>);
}
