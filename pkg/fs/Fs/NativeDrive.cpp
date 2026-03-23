#include "NativeDrive.h"
#include "Log/Log.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Fs
{
    NativeDrive::NativeDrive(std::vector<Path> prefixPaths)
        : _prefixPaths(std::move(prefixPaths))
    {
        if (Log::Enabled(Log::Level::Trace)) {
            std::ostringstream oss;
            oss << "[";
            for (size_t i = 0; i < _prefixPaths.size(); ++i) {
                if (i > 0) {
                    oss << ", ";
                }
                oss << _prefixPaths[i].string();
            }
            oss << "]";
            Log::Trace("{}", oss.str());
        }
    }

    Drive::PathResult NativeDrive::GetNativePath(const Path& path)
    {
        namespace fs = std::filesystem;

        if (path.is_absolute()) {
            if (!fs::exists(path)) {
                return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
            }
            return path;
        }

        std::error_code lastError = std::make_error_code(std::errc::no_such_file_or_directory);

        for (const auto& prefix : _prefixPaths) {
            fs::path fullPath = prefix / path;

            // check file existence to provide better error handling (e.g., distinguish between "not supported" and "file not found")
            if (fs::exists(fullPath)) {
                return fullPath;
            }
        }

        return std::unexpected(lastError);
    }

    Drive::SizeResult NativeDrive::GetSize(const Path& path)
    {
        auto nativePath = GetNativePath(path);
        if (!nativePath) {
            return std::unexpected(nativePath.error());
        }

        std::error_code ec;
        auto size = std::filesystem::file_size(nativePath.value(), ec);
        if (ec) {
            return std::unexpected(ec);
        }
        return size;
    }

    Drive::ReadResult NativeDrive::ReadAllTo(const Path& path, std::vector<uint8_t>& buf)
    {
        auto nativePath = GetNativePath(path);
        if (!nativePath) {
            return std::unexpected(nativePath.error());
        }

        std::ifstream file(nativePath.value(), std::ios::binary);
        if (!file) {
            return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        }

        file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        return static_cast<size_t>(file.gcount());
    }
}
