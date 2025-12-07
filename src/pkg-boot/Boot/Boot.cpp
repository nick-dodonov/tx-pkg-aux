// TODO: replace w/ something from std or mv -D to BUILD.bazel copts
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>

#include "Boot.h"
#include "Build.h"
#include "Log/Log.h"

#include <chrono>
#include <format>

namespace Boot
{
    static constexpr auto AppData = "0001"; // TODO: embedded in elf (may be w/ git tag or similar)

    template <typename T>
    static void Line(T&& msg)
    {
        using namespace Log;
        Info(std::forward<T>(msg), Src{Src::NoFunc});
    }

    template <typename... Args>
    static void Line(std::format_string<Args...>&& fmt, Args&&... args)
    {
        using namespace Log;
        Info({std::move(fmt), Src{Src::NoFunc}}, std::forward<Args>(args)...);
    }

    void LogHeader(const int argc, const char** argv)
    {
        Line("-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>-=>");
        Line("App: {}", AppData);
        Line("Build: {}", Build::BuildDescription());

        // startup time
        const auto now = std::chrono::system_clock::now();
        const auto now_c = std::chrono::system_clock::to_time_t(now);
        std::array<char, 20> buffer = {};
        if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c))) {
            Line("Runtime: {}", buffer.data());
        } else {
            Line("Time: [Error formatting time]");
        }

        std::array<char, 1024> cwd = {};
        if (!getcwd(cwd.data(), cwd.size())) {
            cwd[0] = '\0';
        }
        Line("Directory: {}", cwd.data());

        Line("Command:");
        for (auto i = 0; i < argc; ++i) {
            Line("  [{}] {}", i, argv[i]);
        }

        Line("<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-<=-");
    }
}
