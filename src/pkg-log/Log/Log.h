#pragma once
#include "Level.h"
#include "Src.h"
#include <spdlog/spdlog.h>

namespace Log::Detail
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

    static constexpr auto AreaLoggerLine = -1;
}

namespace Log
{
    template <class... Args>
    struct FmtMsg : std::format_string<Args...>
    {
        Src src;

        // ReSharper disable once CppNonExplicitConvertingConstructor
        template <class Tp>
          requires std::convertible_to<const Tp&, std::basic_string_view<char>>
        consteval FmtMsg(const Tp& str, const Src src = Src::current()) // NOLINT(*-explicit-constructor)
            : std::format_string<Args...>{str}
            , src{src}
        {}
    };

    inline bool Enabled(const Level level)
    {
        return Detail::DefaultLoggerRaw()->should_log(Detail::ToSpdLevel(level));
    }

    template <typename T>
    void Msg(const Level level, T&& msg, const Src src = Src::current())
    {
        Detail::DefaultLoggerRaw()->log(
            Detail::ToSpdLoc(src),
            Detail::ToSpdLevel(level),
            std::forward<T>(msg));
    }

    template <typename... Args>
    void Msg(const Level level, FmtMsg<Args...> fmt, Args&&... args)
    {
        //TODO: static_cast<spdlog::format_string_t<Args...>&>(fmt);
        Detail::DefaultLoggerRaw()->log(
            Detail::ToSpdLoc(fmt.src),
            Detail::ToSpdLevel(level),
            fmt.get(),
            std::forward<Args>(args)...);
    }

    template <typename T>
    void Trace(T&& msg, const Src src = Src::current())
    {
        Msg(Level::Trace, std::forward<T>(msg), src);
    }
    template <typename... Args>
    void Trace(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        Msg(Level::Trace, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename T>
    void Debug(T&& msg, const Src src = Src::current())
    {
        Msg(Level::Debug, std::forward<T>(msg), src);
    }
    template <typename... Args>
    void Debug(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        Msg(Level::Debug, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename T>
    void Info(T&& msg, const Src src = Src::current())
    {
        Msg(Level::Info, std::forward<T>(msg), src);
    }
    template <typename... Args>
    void Info(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        Msg(Level::Info, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename T>
    void Warn(T&& msg, const Src src = Src::current())
    {
        Msg(Level::Warn, std::forward<T>(msg), src);
    }
    template <typename... Args>
    void Warn(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        Msg(Level::Warn, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename T>
    void Error(T&& msg, const Src src = Src::current())
    {
        Msg(Level::Error, std::forward<T>(msg), src);
    }
    template <typename... Args>
    void Error(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        Msg(Level::Error, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename T>
    void Fatal(T&& msg, const Src src = Src::current())
    {
        Msg(Level::Fatal, std::forward<T>(msg), src);
    }
    template <typename... Args>
    void Fatal(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        Msg(Level::Fatal, std::move(fmt), std::forward<Args>(args)...);
    }

    struct Logger
    {
        static Logger Default;

        explicit Logger(const char* area = nullptr)
            : _area(area)
        {}

        bool Enabled(const Level level) {
            return Raw()->should_log(Detail::ToSpdLevel(level));
        }

        template <typename T>
        void Msg(spdlog::source_loc loc, const Level level, T&& msg)
        {
            if (_area) {
                loc.filename = _area;
                loc.line = -1;
            }
            Raw()->log(loc, Detail::ToSpdLevel(level), std::forward<T>(msg));
        }

        template <typename... Args>
        void Msg(spdlog::source_loc loc, const Level level, std::format_string<Args...> fmt, Args&&... args)
        {
            if (_area) {
                loc.filename = _area;
                loc.line = Detail::AreaLoggerLine;
            }
            Raw()->log(loc, Detail::ToSpdLevel(level), fmt.get(), std::forward<Args>(args)...);
        }

        // helpers allowing to use macro w/ passing logger instance as the 1st argument
        template <typename T>
        void Msg(const spdlog::source_loc loc, const Level level, Logger& logger, T&& msg)
        {
            logger.Msg(loc, level, std::forward<T>(msg));
        }

        template <typename... Args>
        void Msg(const spdlog::source_loc loc, const Level level, Logger& logger, std::format_string<Args...> fmt, Args&&... args)
        {
            logger.Msg(loc, level, fmt, std::forward<Args>(args)...);
        }

    private:
        spdlog::logger* Raw() { return Detail::DefaultLoggerRaw(); }

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
