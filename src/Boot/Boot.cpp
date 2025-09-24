#include "Boot.h"
#include "Build.h"
#include "Log/Log.h"
#include <format>

namespace Boot
{
    void LogInfo(int argc, char *argv[])
    {
        Log::Info("Command:");
        for (int i = 0; i < argc; ++i)
        {
            Log::Info(std::format("  Arg[{}]: {}", i, argv[i]));
        }
        Log::Info(std::format("Build: {}", Build::BuildDescription()));
    }
}
