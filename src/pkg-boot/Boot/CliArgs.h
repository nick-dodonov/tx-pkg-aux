#pragma once
#include <expected>
#include <string_view>
#include <system_error>
#include <vector>

namespace Boot
{
    using CliArgsView = std::vector<std::string_view>;

    class CliArgs: public CliArgsView
    {
    public:
        CliArgs(const int argc, char** argv) noexcept
            : CliArgsView(argv, argv + argc)
        {}
        CliArgs(const int argc, const char** argv) noexcept
            : CliArgsView(argv, argv + argc)
        {}

        [[nodiscard]] bool Contains(std::string_view str) const noexcept;
        [[nodiscard]] std::expected<int, std::error_code> GetIntArg(const size_t index) const noexcept;
    };
}
