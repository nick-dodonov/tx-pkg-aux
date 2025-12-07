#include "App/AsioContext.h"
#include "Boot/Boot.h"
#include "Log/Log.h"

namespace asio = boost::asio;
static App::AsioContext asioContext;

static asio::awaitable<void> Sub() {
    Log::Info("Sub started");

    auto timer = asio::steady_timer(co_await asio::this_coro::executor);
    timer.expires_after(std::chrono::seconds(1));
    co_await timer.async_wait(asio::use_awaitable);

    Log::Info("Sub finished");
}

static asio::awaitable<std::string> Foo(int input) {
    Log::Info("Foo started: {}", input);
    co_await Sub(); // emulation for async work
    Log::Info("Foo finished: {}", input);

    co_return "Foo emulated result on input " + std::to_string(input);
}

template <typename CompletionToken>
auto FooAsync(int input, CompletionToken&& token) // NOLINT(cppcoreguidelines-missing-std-forward)
{
    Log::Info("FooAsync request: {}", input);

    // return asio::async_initiate<CompletionToken, void(std::string)>(
    //     [](auto&& handler, int inp) {
    //         // Проверяем, можем ли мы прямо вернуть awaitable
    //         using handler_type = std::decay_t<decltype(handler)>;
    //         // Если handler - это awaitable handler, возвращаем awaitable напрямую
    //         if constexpr (requires { 
    //             typename handler_type::awaitable_type; 
    //         }) {
    //             // Это awaitable-based handler - вызываем напрямую
    //             auto aw = Foo(inp);
    //             // Передаем awaitable в handler (он знает что с ним делать)
    //             return std::forward<decltype(handler)>(handler)(std::move(aw));
    //         } else {
    //             // Обычный handler - используем co_spawn
    //             auto executor = asio::get_associated_executor(handler);
    //             asio::co_spawn(
    //                 executor,
    //                 Foo(inp),
    //                 [h = std::forward<decltype(handler)>(handler)](
    //                     std::exception_ptr ex, std::string result) mutable {
    //                     if (!ex) {
    //                         std::move(h)(std::move(result));
    //                     }
    //                 }
    //             );
    //         }
    //     },
    //     token,
    //     input
    // );

    // // Проверяем результирующий тип
    // using result_type = typename asio::async_result<
    //     std::decay_t<CompletionToken>, 
    //     void(std::string)
    // >::return_type;
    
    using token_type = std::decay_t<CompletionToken>;
    
    constexpr bool is_awaitable = []() {
        if constexpr (requires { typename token_type::as_default_on_t; }) {
            using base_token = token_type::as_default_on_t;
            return std::is_same_v<base_token, asio::use_awaitable_t<>>;
        } else {
            return std::is_same_v<token_type, asio::use_awaitable_t<>>;
        }
    }();
    if constexpr (is_awaitable) {
        // Просто возвращаем awaitable - token сам применит трансформации при co_await
        return Foo(input);
    } else {
        // Для не-awaitable используем co_spawn
        return asio::async_initiate<CompletionToken, void(std::string)>(
            [](auto&& handler, int inp) {
                auto executor = asio::get_associated_executor(handler);
                asio::co_spawn(
                    executor,
                    Foo(inp),
                    [h = std::forward<decltype(handler)>(handler)](const std::exception_ptr& ex,
                    std::string result) mutable {
                        if (!ex) {
                            std::move(h)(std::move(result));
                        }
                    }
                );
            },
            token,
            input
        );
    }

    // // Проверяем, является ли token use_awaitable
    // if constexpr (std::is_same_v<
    //     std::decay_t<CompletionToken>, 
    //     asio::use_awaitable_t<>> ||
    //     std::is_convertible_v<std::decay_t<CompletionToken>, asio::use_awaitable_t<>>)
    // {
    //     // Прямой возврат awaitable - без co_spawn!
    //     return Foo(input);
    // }
    // else
    // {
    //     return asio::async_initiate<CompletionToken, void(std::string)>(
    //         [](auto handler, int inp) {
    //             auto executor = asio::get_associated_executor(handler);
    //             asio::co_spawn(
    //                 executor,
    //                 Foo(inp),
    //                 [h = std::move(handler)](std::exception_ptr ex, std::string result) mutable {
    //                     if (ex) {
    //                         std::rethrow_exception(ex);
    //                     } else {
    //                         std::move(h)(std::move(result));
    //                     }
    //                 }
    //             );
    //         },
    //         token,
    //         input
    //     );
    // }


    // auto executor = asio::get_associated_executor(token);
    // return asio::async_initiate<CompletionToken, void(std::string)>(
    //     asio::co_composed<void(std::string)>([](auto state, int input) -> void {
    //         Log::Info("FooAsync co_composed started: {}", input);
    //         // state.throw_if_cancelled(true);
    //         // state.reset_cancellation_state(asio::enable_terminal_cancellation());
    //         //auto executor = co_await asio::this_coro::executor;
    //         asio::steady_timer timer(asioContext.get_executor());
    //         timer.expires_after(std::chrono::seconds(1));
    //         co_await timer.async_wait(); //asio::use_awaitable);

    //         auto deferred_op = timer.async_wait(asio::deferred);
    //         deferred_op(
    //             [](std::error_code ec) {
    //                 Log::Info("Timer expired");
    //             }
    //         );
            
    //         co_yield state.complete("1111111111");
    //         Log::Info("FooAsync co_composed finished: {}", input);
    //     }, executor),
    //     token,
    //     std::ref(input)
    // );

    // return asio::async_initiate<CompletionToken, void(std::string)>(
    //     [&](auto&& handler, int input) {
            
    //         asio::co_spawn(
    //             asio::get_associated_executor(handler),
    //             Foo(input),
    //             [handler = std::forward<decltype(handler)>(handler)](const std::exception_ptr& ex, std::string&& result) mutable {
    //                 if (!ex) {
    //                     std::move(handler)(std::move(result));
    //                 } else {
    //                     std::move(handler)("ERROR");
    //                 }
    //             }
    //         );
    //     },
    //     token,
    //     std::move(input)
    // );
}

static asio::awaitable<void> CoroMain() {
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
    co_return;
}

void AsioMain()
{
    auto executor = asioContext.get_executor();
    boost::asio::co_spawn(executor, CoroMain, asio::detached);
}

int main(int argc, char* argv[])
{
    Boot::LogHeader(argc, argv);
    auto executor = asioContext.get_executor();

    executor.execute(AsioMain);

    //boost::asio::co_spawn(executor, CoroMain, asio::detached);

    // boost::asio::co_spawn(executor, Sub, boost::asio::detached);
    // auto awaitable = Sub();
    // boost::asio::co_spawn(executor, std::move(awaitable), boost::asio::detached);

    for (int i = 0; i < 1; ++i) {
        Log::Info("Main: tick started: {}", i);
        [[maybe_unused]] auto _ = asioContext.Run();
        Log::Info("Main: tick finished: {}", i);
    }
    return 0;
}
