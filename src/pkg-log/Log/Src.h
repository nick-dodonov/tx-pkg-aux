#pragma once
#include <source_location>
#include <spdlog/common.h>

namespace Log
{
    using Src = std::source_location;
}

namespace Log::Detail
{
    inline spdlog::source_loc ToSpdLoc(const Src src)
    {
        return spdlog::source_loc{
            src.file_name(),
            static_cast<int>(src.line()),
            src.function_name()};
    }
}
