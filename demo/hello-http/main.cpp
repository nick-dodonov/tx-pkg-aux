//#include "Http/SimpleClient.h"
#include "Log/Log.h"
#include <asio.hpp>
#include <functional>

using namespace std::chrono_literals;

static asio::awaitable<void> http_get_async(
    const asio::any_io_executor& executor,
    std::string_view url,
    const std::function<void(std::string_view url)> handler)
{
    asio::ip::tcp::resolver resolver{executor};

    // auto results = resolver.resolve("example.com", {});
    // for (const auto& entry : results) {
    //     auto endpoint = entry.endpoint();
    //     Log::InfoF("Resolved endpoint: {}:{}", endpoint.address().to_string(), endpoint.port());
    // }

    // resolver.async_resolve("google.com", "asdf", [](const std::error_code& ec, const auto& results) {
    //     Log::InfoF("Resolved results: count={} '{}'", results.size(), ec ? ec.message(): "<OK>");
    //     for (const auto& entry : results) {
    //         auto endpoint = entry.endpoint();
    //         Log::InfoF("Resolved endpoint: {}:{}", endpoint.address().to_string(), endpoint.port());
    //     }
    // });

    auto results = co_await resolver.async_resolve("google.com", "80", asio::use_awaitable);
    for (const auto& entry : results) {
        auto endpoint = entry.endpoint();
        Log::InfoF("Resolved endpoint: {}:{}", endpoint.address().to_string(), endpoint.port());
        handler(endpoint.address().to_string());
    }
}

static void http_get(
    const asio::any_io_executor& executor,
    const std::string_view url,
    std::function<void(std::string_view url)> handler)
{
    asio::co_spawn(executor, http_get_async(executor, url, std::move(handler)), asio::detached);
}

int main()
{
    Log::Info("starting");

    const asio::any_io_executor executor = asio::system_executor();

    auto try_http_get = [&executor](std::string_view url) {
        http_get(executor, url, [url](std::string_view result) { 
            Log::InfoF("http_get result: {}: {}", url, result); 
        });
    };
    try_http_get("http://ifconfig.io/");
    try_http_get("https://httpbin.org/headers");

    asio::detail::global<asio::system_context>().join();
    Log::Info("exiting");
    return 0;
}
