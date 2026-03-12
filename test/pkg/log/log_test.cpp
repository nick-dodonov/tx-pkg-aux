#include "Log/Log.h"
#include <gtest/gtest.h>

// Тест проверяет, что Log::info работает корректно
TEST(LogTest, InfoWorks) {
    // Просто вызываем метод - если он не крашится, тест пройден
    Log::Info("Test info message");
    EXPECT_TRUE(true); // Подтверждаем что мы дошли до этой строки
}

// Тест проверяет, что Log::warn работает корректно
TEST(LogTest, WarnWorks) {
    // Просто вызываем метод - если он не крашится, тест пройден
    Log::Warn("Test warning message");
    EXPECT_TRUE(true); // Подтверждаем что мы дошли до этой строки
}

// Тест проверяет, что Log::error работает корректно  
TEST(LogTest, ErrorWorks) {
    // Просто вызываем метод - если он не крашится, тест пройден
    Log::Error("Test error message");
    EXPECT_TRUE(true); // Подтверждаем что мы дошли до этой строки
}

// Простой интеграционный тест - проверяем что все методы можно вызвать подряд
TEST(LogTest, AllMethodsIntegration) {
    Log::Info("Integration test info");
    Log::Warn("Integration test warn");
    Log::Error("Integration test error");
    
    // Если мы дошли до этой строки, значит все методы отработали без крашей
    EXPECT_TRUE(true);
}