#pragma once
#include <expected>
#include <string>
#include <system_error>

namespace Fs
{
    class Drive
    {
    public:
        virtual ~Drive() = default;

        using PathResult = std::expected<std::string, std::error_code>;
        [[nodiscard]] virtual PathResult GetNativePath(std::string_view path) = 0;
    };
}
