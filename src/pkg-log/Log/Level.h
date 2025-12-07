#pragma once
#include <spdlog/common.h>

namespace Log
{
    enum class Level : unsigned char
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };
}

namespace Log::Details
{
    inline spdlog::level::level_enum ToSpdLevel(const Level level)
    {
        // switch (level) {
        //     case Level::Trace: return spdlog::level::trace;
        //     case Level::Debug: return spdlog::level::debug;
        //     case Level::Info: return spdlog::level::info;
        //     case Level::Warn: return spdlog::level::warn;
        //     case Level::Error: return spdlog::level::err;
        //     case Level::Fatal: return spdlog::level::critical;
        // }
        // assert(false && "ToSpdLevel: unknown log level");
        // return spdlog::level::info;

        return static_cast<spdlog::level::level_enum>(static_cast<int>(level));
    }
}
