#pragma once
#include "Log.h"
#include "Time.h"
#include <string>
#include <format>

namespace Log
{
    struct Scope : MsgBase
    {
        using Prefixes = std::pair<std::string_view, std::string_view>;
        constexpr static Prefixes DefaultPrefixes = {">>>", "<<<"};
        constexpr static auto DefaultLevel = Level::Info;

        using Clock = std::chrono::high_resolution_clock;
        std::chrono::time_point<Clock> start;
        const Prefixes prefixes = DefaultPrefixes;
        Level level;
        Src src;
        std::string msg;

        // ReSharper disable CppNonExplicitConvertingConstructor
        template <typename... Args>
        constexpr Scope(
            const Prefixes& prefixes,
            const Level level,
            FmtMsg<std::type_identity_t<Args>...> fmt, 
            Args&&... args) // NOLINT(cppcoreguidelines-missing-std-forward)
            : start{Clock::now()}
            , prefixes{prefixes}
            , level{level}
            , src{std::move(fmt.src)}
            , msg{std::vformat(fmt.format.get(), std::make_format_args(args...))}
        {
            Msg(src, level, "{} {}", prefixes.first, msg);
        }

        explicit Scope(const Prefixes& prefixes, const Level level = DefaultLevel, const Src src = {})
            : start{Clock::now()}
            , prefixes{prefixes}
            , level{level}
            , src{src}
        {
            Msg(src, level, prefixes.first);
        }

        template <typename... Args>
        Scope(
            const Level level,
            FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
            : Scope{DefaultPrefixes, level, std::move(fmt),  std::forward<Args>(args)...}
        {}

        template <typename... Args>
        explicit Scope(
            FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
            : Scope{DefaultLevel, std::move(fmt), std::forward<Args>(args)...}
        {}

        explicit Scope(const Level level = DefaultLevel, const Src src = {})
            : Scope{DefaultPrefixes, level, src}
        {}
        // ReSharper restore CppNonExplicitConvertingConstructor

        ~Scope() noexcept
        {
            auto duration = Clock::now() - start;
            Time::HumanReadableBuffer timeBuf{};
            auto timeStr = Time::GetHumanReadableDuration(duration, timeBuf);
            if (msg.empty()) {
                Msg(src, level, "{} / {}", prefixes.second, timeStr);
            } else {
                Msg(src, level, "{} {} / {}", prefixes.second, msg, timeStr);
            }
        }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;
    };
}
