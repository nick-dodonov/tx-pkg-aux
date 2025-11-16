// TODO: replace w/ something from std or mv -D to BUILD.bazel copts
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
    static constexpr const char* AppData = "0001"; // TODO: embedded in elf (may be w/ git tag or similar)

    void LogHeader(int argc, char** argv)
    {
        Log::Info("-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>");
        Log::Info(std::format("App: {}", AppData));
        Log::Info(std::format("Build: {}", Build::BuildDescription()));

        // startup time
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::array<char, 20> buffer = {};
        if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c))) {
            Log::Info(std::format("Runtime: {}", buffer.data()));
        } else {
            Log::Info("Time: [Error formatting time]");
        }

        std::array<char, 1024> cwd = {};
        if (!getcwd(cwd.data(), cwd.size())) {
            cwd[0] = '\0';
        }
        Log::Info(std::format("Directory: {}", cwd.data()));

        Log::Info("Command:");
        for (int i = 0; i < argc; ++i) {
            Log::Info(std::format("  [{}] {}", i, argv[i]));
        }

        Log::Info("<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-");
    }
}
