#include "Log/Log.h"

#include <format>

int main(int argc, char* argv[])
{
    Log::debug("Test message (debug)");
    Log::info("Test message (info)");
    Log::warn("Test message (warning)");
    Log::error("Test message (error)");
    Log::fatal("Test message (fatal)");

    Log::info(std::format("Command line: {}", argc));
    for (int i = 0; i < argc; ++i) {
        Log::info(std::format("Argument {}: {}", i, argv[i]));
    }

    return argc - 1; // test exit code is passed from emrun
}
