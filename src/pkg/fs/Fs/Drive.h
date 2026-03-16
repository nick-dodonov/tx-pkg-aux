#pragma once
#include <expected>
#include <system_error>
#include <filesystem>

namespace Fs
{
    using Path = std::filesystem::path;

    class Drive
    {
    public:
        virtual ~Drive() = default;
        
        using PathResult = std::expected<Path, std::error_code>;
        [[nodiscard]] virtual PathResult GetNativePath(const Path& path) = 0;
    };
}
