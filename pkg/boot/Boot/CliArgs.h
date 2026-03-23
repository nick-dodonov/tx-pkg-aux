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
        CliArgs() noexcept = default; // {0, nullptr} supported for default initialization
        template<typename T>
            requires std::is_same_v<T, char*> || std::is_same_v<T, const char*>
        CliArgs(const int argc, T* argv) noexcept
            : CliArgsView(argv, argv + argc)
        {}

        [[nodiscard]] bool Contains(std::string_view str) const noexcept;
        [[nodiscard]] std::expected<int, std::error_code> GetIntArg(size_t index) const noexcept;
    };
}
