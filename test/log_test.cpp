#include "Log/Log.h"
#include <gtest/gtest.h>
#include <sstream>
#include <iostream>

// Тест проверяет, что Log::info не вызывает исключений
TEST(LogTest, InfoDoesNotThrow) {
    EXPECT_NO_THROW(Log::Info("Test info message"));
}

// Тест проверяет, что Log::warn не вызывает исключений
TEST(LogTest, WarnDoesNotThrow) {
    EXPECT_NO_THROW(Log::Warn("Test warning message"));
}

// Тест проверяет, что Log::error не вызывает исключений  
TEST(LogTest, ErrorDoesNotThrow) {
    EXPECT_NO_THROW(Log::Error("Test error message"));
}

// Тест проверяет, что DefaultInit работает
TEST(LogTest, DefaultInitWorks) {
    EXPECT_NO_THROW(Log::DefaultInit());
}

// Простой интеграционный тест - проверяем что все методы можно вызвать подряд
TEST(LogTest, AllMethodsIntegration) {
    EXPECT_NO_THROW({
        Log::Info("Integration test info");
        Log::Warn("Integration test warn");
        Log::Error("Integration test error");
    });
}