#pragma once
#include <array>
#include <chrono>
#include <span>
#include <string_view>

namespace Log::Time
{
    using HumanReadableBuffer = std::array<char, 16>;

    [[nodiscard]] std::string_view GetHumanReadableMicroseconds(std::chrono::microseconds duration, std::span<char> buf);

    template <typename Rep, typename Period>
    [[nodiscard]] std::string_view GetHumanReadableDuration(std::chrono::duration<Rep, Period> duration, std::span<char> buf)
    {
        return GetHumanReadableMicroseconds(std::chrono::duration_cast<std::chrono::microseconds>(duration), buf);
    }
}
