#include "CliArgs.h"
#include <algorithm>
#include <charconv>

namespace Boot
{
    [[nodiscard]] bool CliArgs::Contains(const std::string_view str) const noexcept
    {
        return std::ranges::any_of(*this, [&](const auto& arg) { return arg == str; });
    }

    [[nodiscard]] std::expected<int, std::error_code> CliArgs::GetIntArg(const size_t index) const noexcept
    {
        if (index >= this->size()) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        const auto& arg = (*this)[index];
        int value = 0;
        const auto result = std::from_chars(arg.data(), arg.data() + arg.size(), value);
        if (result.ec != std::errc()) {
            return std::unexpected(std::make_error_code(result.ec));
        }
        return value;
    }
}
