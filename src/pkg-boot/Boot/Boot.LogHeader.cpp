#include "Boot.h"
#include "Build.h"
#include "CliArgs.h"

#include "Log/Log.h"
#include <Log/Path.h>

#include <iomanip>

namespace Boot
{
    static Log::Logger _log{"Boot"};

    template <typename... Args>
    static void Line(Log::FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        fmt.src.mode = Log::Src::NoFunc;
        _log.Info(fmt, std::forward<Args>(args)...);
    }

    static std::string_view GetAppName(const CliArgs& args)
    {
        if (args.empty()) {
            return "<unknown>";
        }
        const auto path = args[0];
        return Log::Path::GetWithoutExtension(Log::Path::GetBasename(path));
    }

    void LogHeader(int argc, char** argv) noexcept { LogHeader(CliArgs(argc, argv)); }

    void LogHeader(const int argc, const char** argv) noexcept { LogHeader(CliArgs(argc, argv)); }

    void LogHeader(const CliArgs& args) noexcept
    {
        const auto appName = GetAppName(args);
        Line("║ App: {}", appName);
        Line("║ Build: {}", Build::BuildDescription());

        // startup time
        const auto tm = spdlog::details::os::localtime();
        const auto tzOffset = spdlog::details::os::utc_minutes_offset(tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S ") << "UTC" << (tzOffset >= 0 ? "+" : "-") << std::abs(tzOffset) / 60 << ":00";
        Line("║ Runtime: {}", oss.view());

        std::array<char, 1024> cwd = {};
        if (!getcwd(cwd.data(), cwd.size())) {
            cwd[0] = '\0';
        }
        Line("║ Directory: {}", cwd.data());

        Line("║ Command:");
        for (auto i = 0; i < args.size(); ++i) {
            Line("║  [{}] {}", i, args[i]);
        }
    }
}
