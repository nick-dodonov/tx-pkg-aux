#pragma once
#include <source_location>
#include <spdlog/common.h>

namespace Log
{
    struct Src : std::source_location
    {
        using std::source_location::source_location;

        enum Mode : std::uint8_t
        {
            All = 0,
            NoFunc = 1,
        };

        Mode mode;

        // ReSharper disable once CppNonExplicitConvertingConstructor
        constexpr Src(const Mode mode = All, const std::source_location loc = current()) // NOLINT(*-explicit-constructor)
            : std::source_location(loc)
            , mode{mode}
        {}

        [[nodiscard]] spdlog::source_loc ToSpdLoc() const
        {
            return spdlog::source_loc{
                file_name(),
                static_cast<int>(line()),
                mode & NoFunc ? nullptr: function_name()};
        }
    };
}

namespace Log::Detail
{
    inline spdlog::source_loc ToSpdLoc(const Src& src) { return src.ToSpdLoc(); }
}
