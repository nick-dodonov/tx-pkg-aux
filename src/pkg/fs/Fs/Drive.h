#pragma once
#include "Path.h"

#include <expected>
#include <system_error>
#include <vector>

namespace Fs
{
    class Drive
    {
    public:
        virtual ~Drive() = default;
        
        using PathResult = std::expected<Path, std::error_code>;
        [[nodiscard]] virtual PathResult GetNativePath(const Path& path) = 0;

        using SizeResult = std::expected<size_t, std::error_code>;
        [[nodiscard]] virtual SizeResult GetSize(const Path& path) = 0;

        using ReadResult = std::expected<size_t, std::error_code>;
        [[nodiscard]] virtual ReadResult ReadAllTo(const Path& path, std::vector<uint8_t>& buf) = 0;
    };
}
