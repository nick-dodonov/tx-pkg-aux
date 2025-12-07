#include "Boot.h"
#include "Build.h"
#include "Log/Log.h"
#include <iomanip>
#include <Log/Path.h>

namespace Boot
{
    template <typename... Args>
    static void Line(Log::FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        fmt.src.mode = Log::Src::NoFunc;
        Log::Info(fmt, std::forward<Args>(args)...);
    }

    static std::string_view GetAppName(int argc, const char** argv)
    {
        if (argc <= 0) {
            return "<unknown>";
        }
        const auto* path = argv[0];
        return Log::Path::GetWithoutExtension(Log::Path::GetBasename(path));
    }

    void LogHeader(const int argc, const char** argv)
    {
        const auto appName = GetAppName(argc, argv);
        Line("║ App: {}", appName);
        Line("║ Build: {}", Build::BuildDescription());

        // startup time
        const auto tm = spdlog::details::os::localtime();
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        Line("║ Runtime: {}", oss.view());

        std::array<char, 1024> cwd = {};
        if (!getcwd(cwd.data(), cwd.size())) {
            cwd[0] = '\0';
        }
        Line("║ Directory: {}", cwd.data());

        Line("║ Command:");
        for (auto i = 0; i < argc; ++i) {
            Line("║  [{}] {}", i, argv[i]);
        }
    }
}
