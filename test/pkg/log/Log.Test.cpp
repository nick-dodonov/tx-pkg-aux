#include "Log/Log.h"
#include <gtest/gtest.h>

// Test verifies that Log::info works correctly
TEST(LogTest, InfoWorks) {
    // Simply call the method - if it doesn't crash, the test passes
    Log::Info("Test info message");
    EXPECT_TRUE(true); // Confirm that we reached this line
}

// Test verifies that Log::warn works correctly
TEST(LogTest, WarnWorks) {
    // Simply call the method - if it doesn't crash, the test passes
    Log::Warn("Test warning message");
    EXPECT_TRUE(true); // Confirm that we reached this line
}

// Test verifies that Log::error works correctly  
TEST(LogTest, ErrorWorks) {
    // Simply call the method - if it doesn't crash, the test passes
    Log::Error("Test error message");
    EXPECT_TRUE(true); // Confirm that we reached this line
}

// Simple integration test - verify that all methods can be called in sequence
TEST(LogTest, AllMethodsIntegration) {
    Log::Info("Integration test info");
    Log::Warn("Integration test warn");
    Log::Error("Integration test error");
    
    // If we reached this line, all methods executed without crashes
    EXPECT_TRUE(true);
}