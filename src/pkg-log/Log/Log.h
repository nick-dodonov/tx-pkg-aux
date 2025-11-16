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

    static constexpr int AreaLoggerLine = -1;
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
        //TODO: static_cast<spdlog::format_string_t<Args...>&>(fmt);
        Details::DefaultLoggerRaw()->log(Details::ToSpdLevel(level), fmt.get(), std::forward<Args>(args)...);
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

    struct Logger
    {
        static Logger Default;

        explicit Logger(const char* area = nullptr)
            : _area(area)
        {}

        template <typename T>
        void Msg(spdlog::source_loc loc, Log::Level level, T&& msg)
        {
            if (_area) {
                loc.filename = _area;
                loc.line = -1;
            }
            Raw()->log(loc, Details::ToSpdLevel(level), std::forward<T>(msg));
        }

        template <typename... Args>
        void Msg(spdlog::source_loc loc, Log::Level level, std::format_string<Args...> fmt, Args&&... args)
        {
            if (_area) {
                loc.filename = _area;
                loc.line = Details::AreaLoggerLine;
            }
            Raw()->log(loc, Details::ToSpdLevel(level), fmt.get(), std::forward<Args>(args)...);
        }

        // helpers allowing to use macro w/ passing logger instance as 1st argument
        template <typename T>
        void Msg(spdlog::source_loc loc, Log::Level level, Logger& logger, T&& msg)
        {
            logger.Msg(loc, level, std::forward<T>(msg));
        }

        template <typename... Args>
        void Msg(spdlog::source_loc loc, Log::Level level, Logger& logger, std::format_string<Args...> fmt, Args&&... args)
        {
            logger.Msg(loc, level, fmt, std::forward<Args>(args)...);
        }

    private:
        spdlog::logger* Raw() { return Details::DefaultLoggerRaw(); }

        const char* _area{};
    };

} // namespace Log

inline Log::Logger& DefaultLogger() { return Log::Logger::Default; }

#define LogMsg(level, ...) DefaultLogger().Msg(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, level, __VA_ARGS__) // NOLINT(cppcoreguidelines-macro-usage)
#define LogTrace(...) LogMsg(Log::Level::Trace, __VA_ARGS__) // NOLINT(cppcoreguidelines-macro-usage)
#define LogDebug(...) LogMsg(Log::Level::Debug, __VA_ARGS__) // NOLINT(cppcoreguidelines-macro-usage)
#define LogInfo(...) LogMsg(Log::Level::Info, __VA_ARGS__) // NOLINT(cppcoreguidelines-macro-usage)
#define LogWarn(...) LogMsg(Log::Level::Warn, __VA_ARGS__) // NOLINT(cppcoreguidelines-macro-usage)
#define LogError(...) LogMsg(Log::Level::Error, __VA_ARGS__) // NOLINT(cppcoreguidelines-macro-usage)
#define LogFatal(...) LogMsg(Log::Level::Fatal, __VA_ARGS__) // NOLINT(cppcoreguidelines-macro-usage)
