#pragma once
#include "Coro/Task.h"

#include <filesystem>
#include <expected>
#include <system_error>
#include <vector>

namespace Fs
{
    using Path = std::filesystem::path;

    class Drive
    {
    public:
        virtual ~Drive() = default;
        
        using PathResult = std::expected<Path, std::error_code>;
        [[nodiscard]] virtual PathResult GetNativePath(const Path& path) = 0;

        //TODO: research capy buffer usage
        using ReadAllBytesResult = std::expected<std::vector<uint8_t>, std::error_code>;
        [[nodiscard]] virtual Coro::Task<ReadAllBytesResult> ReadAllBytesAsync(const Path& path);
    };
}
