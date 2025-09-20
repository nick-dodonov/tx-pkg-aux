#include "Log/Log.h"

int main() {
    // Тестируем функциональность нашего логгера
    Log::info("Test message from lwlog master branch");
    Log::warn("Warning test message");
    Log::error("Error test message");
    
    return 0;
}
