#include "Log/Log.h"

#include <format>

int main(int argc, char* argv[])
{
    Log::Debug("Test message (debug)");
    Log::Info("Test message (info)");
    Log::Warn("Test message (warning)");
    Log::Error("Test message (error)");
    Log::Fatal("Test message (fatal)");

    Log::Info("Command:");
    for (int i = 0; i < argc; ++i) {
        Log::Info(std::format("  Arg[{}]: {}", i, argv[i]));
    }
    auto exitCode = argc - 1;
    Log::Info(std::format("Exit: {}", exitCode));

    return exitCode; // test exit code is passed from emrun
}
