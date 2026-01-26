#include "Boot.h"
#include "Build.h"

#include "Log/Log.h"
#include <Log/Path.h>

#include <Build/Info.h>

#include <iomanip>
#include <filesystem>
#include <string_view>

//TODO: add important env variables logging on start
//#include <cstdlib>
//extern char** environ;

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

    void LogHeader(const CliArgs& args) noexcept
    {
        const auto appName = GetAppName(args);
        Line("║ App: {}", appName);
        Line("║ Compile: {}", Build::BuildDescription());

        // Build info: time branch sha[7:] user@host
        {
            std::string_view sha = Build::Info::GitSha();
            std::string_view branch = Build::Info::GitBranch();
            std::string_view status = Build::Info::GitStatus();
            std::string_view user = Build::Info::BuildUser();
            std::string_view host = Build::Info::BuildHost();
            std::string_view time = Build::Info::BuildTime();

            // Get first 7 chars of SHA
            const auto sha7 = sha.substr(0, std::min<size_t>(7, sha.size()));

            // Add * prefix if status is dirty
            const bool isDirty = status == "dirty";
            
            // Split time "2026-01-26T21:50:00+01:00" into 3 parts without allocation
            const auto tPos = time.find('T');
            const auto tzPos = time.find_last_of("+-");
            const auto date = time.substr(0, tPos);
            const auto timeOfDay = time.substr(tPos + 1, tzPos - tPos - 1);
            const auto timezone = time.substr(tzPos);

            Line("║ Version: {} {} {} | {} {}{} | {}@{}", 
                date, timeOfDay, timezone, branch, 
                isDirty ? "*" : "", sha7,
                user, host);
        }

        // startup time
        const auto tm = spdlog::details::os::localtime();
        const auto tzOffset = spdlog::details::os::utc_minutes_offset(tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S ") 
            << (tzOffset >= 0 ? "+" : "-") 
            << std::setfill('0') << std::setw(2) << std::abs(tzOffset) / 60 << ":00";
        Line("║ Runtime: {}", oss.view());

        // current working directory
        std::error_code ec;
        const auto currentPath = std::filesystem::current_path(ec);
        if (!ec) {
            Line("║ Directory: {}", currentPath.string());
        } else {
            Line("║ Directory: <error: {}>", ec.message());
        }

        Line("║ Command:");
        for (size_t i = 0; i < args.size(); ++i) {
            Line("║  [{}] {}", i, args[i]);
        }

        //TODO: add important env variables logging on start
        // // environment variables
        // Line("║ Environment:");
        // const std::vector<std::string_view> prefixes = {"BUILD_", "RUNFILES_", "WORKSPACE_"};
        // for (char** env = environ; *env != nullptr; ++env) {
        //     const std::string_view envStr(*env);
        //     //Line("║  {}", envStr);
        //     for (const auto& prefix : prefixes) {
        //         if (envStr.starts_with(prefix)) {
        //             Line("║  {}", envStr);
        //             break;
        //         }
        //     }
        // }
    }
}
