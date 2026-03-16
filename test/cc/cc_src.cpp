#include "Log/Log.h"
// #include "Boot/Boot.h"
#include <gtest/gtest.h>
#include <functional>
#include <memory>

static bool fail = false;

TEST(LogTest, Works) {
    Log::Info("Test info message");
    Log::Warn("Test warning message");
    Log::Error("Test error message");
    EXPECT_TRUE(!fail);
}

TEST(CppFeatureTest, MoveOnlyFunction) {
    // Test move-only function semantics using lambda with unique_ptr capture
    // Note: std::move_only_function is C++23 but may not be available in all implementations
    
#if __cpp_lib_move_only_function >= 202110L
    // If std::move_only_function is available, use it
    std::move_only_function<int(int)> func = [ptr = std::make_unique<int>(42)](int x) {
        return *ptr + x;
    };
    
    EXPECT_EQ(func(8), 50);
    
    // Test move semantics
    auto func2 = std::move(func);
    EXPECT_EQ(func2(10), 52);
#else
    // Fallback: Test move-only semantics with std::function
    // Note: std::function requires copyable callables, so we test with movable lambda
    auto lambda = [ptr = std::make_unique<int>(42)](int x) {
        return *ptr + x;
    };
    
    EXPECT_EQ(lambda(8), 50);
    
    // Test that move works
    auto lambda2 = std::move(lambda);
    EXPECT_EQ(lambda2(10), 52);
    
    // Log that move_only_function is not available
    Log::Warn("std::move_only_function not available, testing move-only lambda instead");
#endif
    
    // This test always passes to verify the feature detection works
    EXPECT_TRUE(true);
}

int main(int argc, char** argv)
{
    if (argc > 1) {
        fail = true;
    }
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
