#include "Boot.h"
#include "Build.h"
#include "Log/Log.h"
#include <chrono>
#include <ctime>
#include <format>

namespace Boot
{
    static const char* AppData = "0000"; //TODO: embedded in elf (may be w/ git tag or similar)
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

        Log::Info("Command:");
        for (int i = 0; i < argc; ++i) {
            Log::Info(std::format("  [{}] {}", i, argv[i]));
        }
        //TODO: cwd
        Log::Info("<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-");
    }
}
