#pragma once
#include <algorithm>
#include <string_view>
#include <vector>

namespace Boot
{
    using CliArgsView = std::vector<std::string_view>;

    class CliArgs : public CliArgsView
    {
    public:
        CliArgs(const int argc, char** argv) noexcept
            : CliArgsView(argv, argv + argc)
        {}
        CliArgs(const int argc, const char** argv) noexcept
            : CliArgsView(argv, argv + argc)
        {}

        [[nodiscard]] bool Contains(const std::string_view str) const noexcept
        {
            return std::ranges::any_of(*this, [&](const auto& arg) { return arg == str; });
        }
    };
}
