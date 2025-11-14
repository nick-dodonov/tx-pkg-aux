#include "Log/Log.h"
#include <format>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[])
{
    spdlog::info("Welcome to spdlog!");
    spdlog::error("Some error message with arg: {}", 1);

    Log::Debug("Test message (debug)");
    Log::Info("Test message (info)");
    Log::Warn("Test message (warning)");
    Log::Error("Test message (error)");
    Log::Fatal("Test message (fatal)");

    Log::Fatal(std::format("  DEBUG compile_commands: {}", _LIBCPP_STD_VER));

    Log::Info("Command:");
    for (int i = 0; i < argc; ++i) {
        Log::Info(std::format("  Arg[{}]: {}", i, argv[i]));
    }

    auto exitCode = argc - 1;
    Log::Info(std::format("Exit: {}", exitCode));

    return exitCode; // test exit code is passed from emrun
}
