#include "App/AsioContext.h"
#include "Log/Log.h"

namespace asio = boost::asio;
static App::AsioContext asioContext;

static asio::awaitable<void> Sub()
{
    Log::Info("Sub started");

    auto timer = asio::steady_timer(co_await asio::this_coro::executor);
    timer.expires_after(std::chrono::milliseconds(300));
    co_await timer.async_wait(asio::use_awaitable);

    Log::Info("Sub finished");
}

static asio::awaitable<std::string> FooImpl(int input)
{
    Log::Info("Foo started: {}", input);

    co_await Sub(); // emulation for async work

    Log::Info("Foo finished: {}", input);
    co_return "Foo emulated result on input " + std::to_string(input);
}

template <typename CompletionToken>
auto FooAsync(int input, CompletionToken&& token) // NOLINT(cppcoreguidelines-missing-std-forward)
{
    Log::Info("FooAsync request: {}", input);
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
    Log::Info("CoroMain started");
    {
        auto awaitable = FooAsync(1, asio::use_awaitable);
        auto result = co_await std::move(awaitable);
        Log::Info("CoroMain: FooAsync(1) result: {}", result);
    }
    {
        boost::system::error_code ec;
        auto awaitable = FooAsync(2, asio::redirect_error(asio::use_awaitable, ec));
        auto result = co_await std::move(awaitable);
        Log::Info("CoroMain: FooAsync(2) result: {}", result);
    }
    {
        auto awaitable = FooAsync(3, asio::as_tuple(asio::use_awaitable));
        auto [result] = co_await std::move(awaitable);
        Log::Info("CoroMain: FooAsync(3) result: {}", result);
    }
    Log::Info("CoroMain finished");
    co_return 111;
}

int main(const int argc, const char* argv[])
{
    return asioContext.RunCoroMain(argc, argv, CoroMain());
}
