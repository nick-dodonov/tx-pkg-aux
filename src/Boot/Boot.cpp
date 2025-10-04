//TODO: replace w/ something from std or mv -D to BUILD.bazel copts
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>

#include "Boot.h"
#include "Build.h"
#include "Log/Log.h"

#include <chrono>
#include <ctime>
#include <format>

namespace Boot
{
    static const char* AppData = "0001"; //TODO: embedded in elf (may be w/ git tag or similar)

    void LogInfo(int argc, char* argv[])
    {
        Log::Info("-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>");
        Log::Info(std::format("App: {}", AppData));
        Log::Info(std::format("Build: {}", Build::BuildDescription()));

        // startup time
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        char buffer[20];
        if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c))) {
            Log::Info(std::format("Runtime: {}", buffer));
        } else {
            Log::Info("Time: [Error formatting time]");
        }

        char cwd[1024];
        if (!getcwd(cwd, sizeof(cwd))) {
            cwd[0] = '\0';
        }
        Log::Info(std::format("Directory: {}", cwd));

        Log::Info("Command:");
        for (int i = 0; i < argc; ++i) {
            Log::Info(std::format("  [{}] {}", i, argv[i]));
        }

        Log::Info("<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-");
    }
}
