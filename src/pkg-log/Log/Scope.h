#pragma once
#include "Log.h"
#include <string>
#include <format>

namespace Log
{
    struct Scope : MsgBase
    {
        using Prefixes = std::pair<std::string_view, std::string_view>;
        constexpr static Prefixes DefaultPrefixes = {">>>", "<<<"};
        constexpr static auto DefaultLevel = Level::Info;

        const Prefixes prefixes = DefaultPrefixes;
        Level level;
        Src src;
        std::string msg;

        // ReSharper disable CppNonExplicitConvertingConstructor
        template <typename... Args>
        constexpr Scope( // NOLINT(*-explicit-constructor)
            const Prefixes& prefixes,
            const Level level,
            FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args) // NOLINT(cppcoreguidelines-missing-std-forward)
            : prefixes(prefixes)
            , level{level}
            , src{std::move(fmt.src)}
            , msg{std::vformat(fmt.get(), std::make_format_args(args...))}
        {
            Msg(src, level, "{} {}", prefixes.first, msg);
        }

        template <typename... Args>
        constexpr Scope( // NOLINT(*-explicit-constructor)
            const Level level,
            FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
            : Scope{DefaultPrefixes, level, std::move(fmt),  std::forward<Args>(args)...}
        {}

        template <typename... Args>
        constexpr Scope( // NOLINT(*-explicit-constructor)
            FmtMsg<std::type_identity_t<Args>...> fmt, Args&&... args)
            : Scope{DefaultLevel, std::move(fmt), std::forward<Args>(args)...}
        {}

        constexpr Scope(const Prefixes& prefixes, const Level level = DefaultLevel, const Src src = {})
            : prefixes{prefixes}
            , level{level}
            , src{src}
        {
            Msg(src, level, prefixes.first);
        }

        constexpr Scope(const Level level = DefaultLevel, const Src src = {})
            : Scope{DefaultPrefixes, level, src}
        {}

        // ReSharper restore CppNonExplicitConvertingConstructor

        ~Scope() noexcept
        {
            Msg(src, level, "{} {}", prefixes.second, msg);
        }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;
    };
}
