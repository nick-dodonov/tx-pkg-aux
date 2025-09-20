#include "Log/Log.h"

#include <format>

int main(int argc, char* argv[])
{
    Log::info("Test message from lwlog master branch");
    Log::warn("Warning test message");
    Log::error("Error test message");

    Log::info(std::format("Command line arguments count: {}", argc));
    for (int i = 0; i < argc; ++i) {
        Log::info(std::format("Argument {}: {}", i, argv[i]));
    }

    return 0;
}
