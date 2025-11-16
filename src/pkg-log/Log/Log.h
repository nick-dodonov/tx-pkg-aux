#pragma once
#include "Level.h"
#include <spdlog/spdlog.h>

namespace Log::Details
{
    void DefaultInit();
    inline spdlog::logger* DefaultLoggerRaw()
    {
        static spdlog::logger* _logger;
        if (_logger) {
            return _logger;
        }
        DefaultInit();
        return _logger = spdlog::default_logger_raw();
    }

    inline spdlog::level::level_enum ToSpdLevel(Level level);
}

namespace Log
{
    template <typename T>
    void Msg(Level level, T&& msg)
    {
        Details::DefaultLoggerRaw()->log(Details::ToSpdLevel(level), std::forward<T>(msg));
    }

    template <typename... Args>
    void Msg(Level level, std::format_string<Args...> fmt, Args&&... args)
    {
        // Convert std::format_string to string_view for spdlog
        auto spdfmt = fmt.get(); //TODO: static_cast<spdlog::format_string_t<Args...>&>(fmt);
        Details::DefaultLoggerRaw()->log(Details::ToSpdLevel(level), spdfmt, std::forward<Args>(args)...);
    }

    template <typename T>
    void Trace(T&& msg)
    {
        Msg(Level::Trace, std::forward<T>(msg));
    }
    template <typename... Args>
    void Trace(std::format_string<Args...> fmt, Args&&... args)
    {
        Msg(Level::Trace, fmt, std::forward<Args>(args)...);
    }

    template <typename T>
    void Debug(T&& msg)
    {
        Msg(Level::Debug, std::forward<T>(msg));
    }
    template <typename... Args>
    void Debug(std::format_string<Args...> fmt, Args&&... args)
    {
        Msg(Level::Debug, fmt, std::forward<Args>(args)...);
    }

    template <typename T>
    void Info(T&& msg)
    {
        Msg(Level::Info, std::forward<T>(msg));
    }
    template <typename... Args>
    void Info(std::format_string<Args...> fmt, Args&&... args)
    {
        Msg(Level::Info, fmt, std::forward<Args>(args)...);
    }

    template <typename T>
    void Warn(T&& msg)
    {
        Msg(Level::Warn, std::forward<T>(msg));
    }
    template <typename... Args>
    void Warn(std::format_string<Args...> fmt, Args&&... args)
    {
        Msg(Level::Warn, fmt, std::forward<Args>(args)...);
    }

    template <typename T>
    void Error(T&& msg)
    {
        Msg(Level::Error, std::forward<T>(msg));
    }
    template <typename... Args>
    void Error(std::format_string<Args...> fmt, Args&&... args)
    {
        Msg(Level::Error, fmt, std::forward<Args>(args)...);
    }

    template <typename T>
    void Fatal(T&& msg)
    {
        Msg(Level::Fatal, std::forward<T>(msg));
    }
    template <typename... Args>
    void Fatal(std::format_string<Args...> fmt, Args&&... args)
    {
        Msg(Level::Fatal, fmt, std::forward<Args>(args)...);
    }
}

namespace Log::Details
{
    inline spdlog::level::level_enum ToSpdLevel(Level level)
    {
        switch (level) {
            case Level::Trace: return spdlog::level::trace;
            case Level::Debug: return spdlog::level::debug;
            case Level::Info: return spdlog::level::info;
            case Level::Warn: return spdlog::level::warn;
            case Level::Error: return spdlog::level::err;
            case Level::Fatal: return spdlog::level::critical;
        }
        assert(false && "ToSpdLevel: unknown log level");
        return spdlog::level::info;
    }
}
