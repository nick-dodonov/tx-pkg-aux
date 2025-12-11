#pragma once
#include "Level.h"
#include "Src.h"
#include <spdlog/spdlog.h>

namespace Log::Detail
{
    void DefaultInit();

    inline spdlog::logger* DefaultLoggerRaw() noexcept
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
    struct MsgBase
    {
        Src src;
    };

    template <class... Args>
    struct FmtMsg : std::format_string<Args...>, MsgBase // order changed to fix MSVC issue (consteval ctor first)
    {
        // ReSharper disable CppNonExplicitConvertingConstructor
        template <class Tp>
          requires std::convertible_to<const Tp&, std::basic_string_view<char>>
        constexpr FmtMsg(const Tp& str, const Src src = {}) noexcept // NOLINT(*-explicit-constructor)
            : std::format_string<Args...>{str}
            , MsgBase{src}
        {}
        // ReSharper restore CppNonExplicitConvertingConstructor
    };

    template <>
    struct FmtMsg<> : std::string_view, MsgBase
    {
        template <typename T>
            requires (!std::same_as<std::decay_t<T>, FmtMsg> && std::constructible_from<std::string_view, T>)
        // ReSharper disable once CppNonExplicitConvertingConstructor
        constexpr FmtMsg(T&& msg, const Src src = {}) noexcept // NOLINT(*-explicit-constructor)
            : std::string_view{std::forward<T>(msg)}
            , MsgBase{src}
        {}

        [[nodiscard]] constexpr std::string_view get() const noexcept { return {data(), size()}; }
    };

    inline bool Enabled(const Level level)
    {
        return Detail::DefaultLoggerRaw()->should_log(Detail::ToSpdLevel(level));
    }

    template <typename... Args>
    void Msg(const Src src, const Level level, std::string_view fmt, Args&&... args) noexcept
    {
        Detail::DefaultLoggerRaw()->log(
            Detail::ToSpdLoc(src),
            Detail::ToSpdLevel(level),
            fmt,
            std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Msg(const Level level, FmtMsg<Args...> fmt, Args&&... args) noexcept
    {
        Msg(fmt.src, level, fmt.get(), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Trace(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
    {
        Msg(Level::Trace, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Debug(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
    {
        Msg(Level::Debug, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Info(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
    {
        Msg(Level::Info, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Warn(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
    {
        Msg(Level::Warn, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Error(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
    {
        Msg(Level::Error, std::move(fmt), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Fatal(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
    {
        Msg(Level::Fatal, std::move(fmt), std::forward<Args>(args)...);
    }

    struct Logger
    {
        static Logger Default;

        explicit Logger(const char* area = nullptr) noexcept
            : _area(area)
        {}

        bool Enabled(const Level level) noexcept
        {
            return Raw()->should_log(Detail::ToSpdLevel(level));
        }

        // raw helpers for macros usage
        template <typename T>
        void Msg(spdlog::source_loc loc, const Level level, T&& msg) noexcept
        {
            if (_area) {
                loc.filename = _area;
                loc.line = -1;
            }
            Raw()->log(loc, Detail::ToSpdLevel(level), std::forward<T>(msg));
        }

        template <typename... Args>
        void Msg(spdlog::source_loc loc, const Level level, std::format_string<Args...> fmt, Args&&... args) noexcept
        {
            if (_area) {
                loc.filename = _area;
                loc.line = Detail::AreaLoggerLine;
            }
            Raw()->log(loc, Detail::ToSpdLevel(level), fmt.get(), std::forward<Args>(args)...);
        }

        // raw helpers allowing to use macro w/ passing logger instance as the 1st argument
        template <typename T>
        void Msg(const spdlog::source_loc loc, const Level level, Logger& logger, T&& msg) noexcept
        {
            logger.Msg(loc, level, std::forward<T>(msg));
        }

        template <typename... Args>
        void Msg(const spdlog::source_loc loc, const Level level, Logger& logger, std::format_string<Args...> fmt, Args&&... args) noexcept
        {
            logger.Msg(loc, level, fmt, std::forward<Args>(args)...);
        }

        // main methods
        template <typename... Args>
        void Msg(const Src src, const Level level, std::string_view fmt, Args&&... args) noexcept
        {
            auto loc = Detail::ToSpdLoc(src);
            if (_area) {
                loc.filename = _area;
                loc.line = -1;
            }

            Raw()->log(
                std::move(loc),
                Detail::ToSpdLevel(level),
                fmt,
                std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Msg(const Level level, FmtMsg<Args...> fmt, Args&&... args) noexcept
        {
            Msg(fmt.src, level, fmt.get(), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Trace(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
        {
            Msg(Level::Trace, std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Debug(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
        {
            Msg(Level::Debug, std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Info(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
        {
            Msg(Level::Info, std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Warn(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
        {
            Msg(Level::Warn, std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Error(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
        {
            Msg(Level::Error, std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Fatal(FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
        {
            Msg(Level::Fatal, std::move(fmt), std::forward<Args>(args)...);
        }

    private:
        // ReSharper disable once CppMemberFunctionMayBeStatic
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
