#include "Boot/Boot.h"
#include "App/Domain.h"
#include "App/Loop/Factory.h"
#include "Log/Scope.h"
#include <cstddef>

namespace asio = boost::asio;

static asio::awaitable<void> Sub()
{
    auto _ = Log::Scope{};

    auto timer = asio::steady_timer(co_await asio::this_coro::executor);
    timer.expires_after(std::chrono::milliseconds(150));
    co_await timer.async_wait(asio::use_awaitable);
}

static asio::awaitable<std::string> FooImpl(int input)
{
    auto _ = Log::Scope{Log::Scope::Prefixes{">>> started", "<<< finished"}, Log::Level::Debug, "({})", input};

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

static asio::awaitable<int> CoroMain(int exitCode)
{
    auto _ = Log::Scope{"."};
    {
        auto result = co_await FooAsync(1, asio::use_awaitable);
        Log::Info("FooAsync(1): {}", result);
    }
    {
        boost::system::error_code ec;
        auto result = co_await FooAsync(2, asio::redirect_error(asio::use_awaitable, ec));
        Log::Info("FooAsync(2): {}", result);
    }
    {
        auto [result] = co_await FooAsync(3, asio::as_tuple(asio::use_awaitable));
        Log::Info("FooAsync(3): {}", result);
    }

    Log::Info("exiting with code: {}", exitCode);
    co_return exitCode;
}

int main(const int argc, const char* argv[])
{
    auto args = Boot::DefaultInit(argc, argv);
    auto domain = std::make_shared<App::Domain>();
    auto runner = App::Loop::CreateDefaultRunner(domain);
    auto exitCode = args.GetIntArg(1).value_or(0);
    return domain->RunCoroMain(runner, CoroMain(exitCode));
}
