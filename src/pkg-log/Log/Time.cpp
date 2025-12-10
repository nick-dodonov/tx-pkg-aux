#include "Time.h"

namespace Log::Time
{
    std::string_view GetHumanReadableMicroseconds(std::chrono::microseconds duration, std::span<char> buf)
    {
        using namespace std::chrono;
        auto us = duration.count();

        std::format_to_n_result<char*> result{};
        if (us >= 1'000'000) {
            // >= 1 second - output seconds
            auto sec = static_cast<double>(us) / 1'000'000.0;
            if (sec >= 100.0) {
                result = std::format_to_n(buf.data(), static_cast<std::ptrdiff_t>(buf.size()), "{:.0f} sec", sec);
            }
            else {
                result = std::format_to_n(buf.data(), static_cast<std::ptrdiff_t>(buf.size()), "{:.3g} sec", sec);
            }
        }
        else if (us >= 1'000) {
            // >= 1 millisecond - output milliseconds
            auto ms = static_cast<double>(us) / 1'000.0;
            if (ms >= 100.0) {
                result = std::format_to_n(buf.data(), static_cast<std::ptrdiff_t>(buf.size()), "{:.0f} ms", ms);
            }
            else {
                result = std::format_to_n(buf.data(), static_cast<std::ptrdiff_t>(buf.size()), "{:.3g} ms", ms);
            }
        }
        else {
            // < 1 millisecond - output microseconds
            result = std::format_to_n(buf.data(), static_cast<std::ptrdiff_t>(buf.size()), "{} µs", us);
        }
        
        return {buf.data(), static_cast<size_t>(result.out - buf.data())};
    }
}
