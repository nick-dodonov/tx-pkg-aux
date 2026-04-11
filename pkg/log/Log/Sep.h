#pragma once
#include <array>
#include <concepts>
#include <format>
#include <string_view>

namespace Log
{
    /// Wrapper for formatting integral values with `'` digit separators.
    /// Usage: std::format("{}", Sep{-2286995118}) → "-2'286'995'118"
    //TODO: extend to std::floating_point (format integer part with separators, keep decimal part as-is)
    template <std::integral T>
    struct Sep
    {
        T value;
    };

    template <std::integral T>
    Sep(T) -> Sep<T>;
}

template <std::integral T>
struct std::formatter<Log::Sep<T>>
{
    constexpr auto parse(format_parse_context& ctx) { return _underlying.parse(ctx); }

    auto format(const Log::Sep<T>& s, format_context& ctx) const
    {
        // Max digits: 20 (uint64_t) + 6 separators + 1 sign + 1 null = 28
        std::array<char, 28> buf{};
        auto* end = buf.data() + buf.size();
        auto* pos = end;

        using Unsigned = std::make_unsigned_t<T>;
        const bool negative = std::is_signed_v<T> && s.value < 0;

        // Work with unsigned to handle INT_MIN / INT64_MIN without overflow
        Unsigned abs = negative
            ? static_cast<Unsigned>(0) - static_cast<Unsigned>(s.value)
            : static_cast<Unsigned>(s.value);

        if (abs == 0) {
            *--pos = '0';
        } else {
            int digits = 0;
            while (abs > 0) {
                if (digits > 0 && digits % 3 == 0) {
                    *--pos = '\'';
                }
                *--pos = static_cast<char>('0' + abs % 10);
                abs /= 10;
                ++digits;
            }
        }

        if (negative) {
            *--pos = '-';
        }

        const std::string_view result(pos, static_cast<std::size_t>(end - pos));
        return _underlying.format(result, ctx);
    }

private:
    std::formatter<std::string_view> _underlying;
};
