#include "NativeDrive.h"
#include "Log/Log.h"
#include <filesystem>

namespace Fs
{
    NativeDrive::NativeDrive(std::vector<std::string> prefixPaths)
        : _prefixPaths(std::move(prefixPaths))
    {
        for (const auto& prefix : _prefixPaths) {
            Log::Trace("{}", prefix);
        }
    }

    Drive::PathResult NativeDrive::GetNativePath(std::string_view pathStr)
    {
        namespace fs = std::filesystem;

        fs::path path{pathStr};
        if (path.is_absolute()) {
            if (!fs::exists(path)) {
                return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
            }
            return path.generic_string();
        }

        std::error_code lastError = std::make_error_code(std::errc::no_such_file_or_directory);

        for (const auto& prefix : _prefixPaths) {
            fs::path fullPath = fs::path(prefix) / path;

            // check file existence to provide better error handling (e.g., distinguish between "not supported" and "file not found")
            if (fs::exists(fullPath)) {
                return fullPath.generic_string();
            }
        }

        return std::unexpected(lastError);
    }
}
