#pragma once
#include "Log.h"
#include <string>
#include <format>

namespace Log
{
    struct Scope : MsgBase
    {
        Level level;
        Src src;
        std::string msg;

        // ReSharper disable CppNonExplicitConvertingConstructor
        constexpr Scope(const Level level, const Src src = {})
            : level{level}
            , src{src}
        {
            Msg(src, level, ">>>");
        }

        template <typename... Args>
        constexpr Scope( // NOLINT(*-explicit-constructor)
            const Level level,
            FmtMsg<std::type_identity_t<Args>...> fmt,
            Args&&... args) // NOLINT(cppcoreguidelines-missing-std-forward)
            : level{level}
            , src{std::move(fmt.src)}
            , msg{std::vformat(fmt.get(), std::make_format_args(args...))}
        {
            Msg(src, level, ">>> {}", msg);
        }

        // ReSharper restore CppNonExplicitConvertingConstructor

        ~Scope() noexcept
        {
            Msg(src, level, "<<< {}", msg);
        }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;
    };
}
