#include "App/AsioContext.h"
#include "Log/Log.h"

namespace asio = boost::asio;

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

static asio::awaitable<void> Sub()
{
    auto _ = Log::Scope{Log::Level::Info};

    auto timer = asio::steady_timer(co_await asio::this_coro::executor);
    timer.expires_after(std::chrono::milliseconds(300));
    co_await timer.async_wait(asio::use_awaitable);
}

static asio::awaitable<std::string> FooImpl(int input)
{
    auto _ = Log::Scope{Log::Level::Info, "({})", input};

    co_await Sub(); // emulation for async work

    co_return "Foo emulated result on input " + std::to_string(input);
}

template <typename CompletionToken>
auto FooAsync(int input, CompletionToken&& token) // NOLINT(cppcoreguidelines-missing-std-forward)
{
    Log::Info("request: {}", input);
    return asio::async_initiate<CompletionToken, void(std::string)>(
        []<typename T0>(T0&& handler, const int input_) {
            auto exec = asio::get_associated_executor(handler);
            auto slot = asio::get_associated_cancellation_slot(handler);
            asio::co_spawn(
                exec,
                FooImpl(input_),
                asio::bind_cancellation_slot(
                    slot,
                    [h = std::forward<T0>(handler)](const std::exception_ptr& ex, std::string result) mutable {
                        if (!ex) {
                            std::move(h)(std::move(result));
                        } else {
                            std::rethrow_exception(ex);
                        }
                    }
                )
            );
        },
        token,
        input
    );
}

static asio::awaitable<int> CoroMain()
{
    auto _ = Log::Scope{Log::Level::Info};

    {
        auto awaitable = FooAsync(1, asio::use_awaitable);
        auto result = co_await std::move(awaitable);
        Log::Info("FooAsync(1) result: {}", result);
    }
    {
        boost::system::error_code ec;
        auto awaitable = FooAsync(2, asio::redirect_error(asio::use_awaitable, ec));
        auto result = co_await std::move(awaitable);
        Log::Info("FooAsync(2) result: {}", result);
    }
    {
        auto awaitable = FooAsync(3, asio::as_tuple(asio::use_awaitable));
        auto [result] = co_await std::move(awaitable);
        Log::Info("FooAsync(3) result: {}", result);
    }
    co_return 111;
}

int main(const int argc, const char* argv[])
{
    App::AsioContext asioContext;
    return asioContext.RunCoroMain(argc, argv, CoroMain());
}
