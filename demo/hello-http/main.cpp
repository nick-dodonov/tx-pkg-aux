//#include "Http/SimpleClient.h"
#include "Log/Log.h"
#include <asio.hpp>
#include <ada.h>
#include <expected>
#include <functional>

using namespace std::chrono_literals;

class SimpleClient {
public:
    using Result = std::expected<std::string, std::error_code>;
    using Callback = std::function<void(Result result)>;

    static void Get(
        const asio::any_io_executor& executor,
        const std::string_view url,
        Callback handler)
    {
        asio::co_spawn(executor, GetAsync(executor, url, std::move(handler)), asio::detached);
    }

private:
    static asio::awaitable<void> GetAsync(
        const asio::any_io_executor& executor,
        std::string_view url,
        Callback handler)
    {
        Log::DebugF("http: query: '{}'", url);
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

        auto url_ec = ada::parse<ada::url_aggregator>(url);
        if (!url_ec) {
            auto& error = url_ec.error();
            Log::ErrorF("http: failed to parse: {}", static_cast<uint8_t>(error));
            handler(std::unexpected(std::make_error_code(std::errc::invalid_argument)));
            co_return;
        }

        auto& url_result = url_ec.value();
        Log::DebugF("http: get_protocol: {}", url_result.get_protocol());
        Log::DebugF("http: get_hostname: {}", url_result.get_hostname());
        Log::DebugF("http: get_port: {}", url_result.get_port());
        Log::DebugF("http: get_pathname: {}", url_result.get_pathname());

#if !__EMSCRIPTEN__
        auto url_protocol = url_result.get_protocol();
        auto url_port = url_result.get_port();
        auto url_service = !url_port.empty()
            ? url_port
            : !url_protocol.empty() 
                ? url_protocol.substr(0, url_protocol.size()-1) // resolver supports "http" and "https" services
                : "80"; // default
        auto results = co_await resolver.async_resolve(url_result.get_hostname(), url_service, asio::use_awaitable);
        //Log::DebugF("Resolved endpoints: count={} '{}'", results.size(), ec ? ec.message(): "<OK>");
        for (const auto& entry : results) {
            auto endpoint = entry.endpoint();
            Log::DebugF("http: resolved: {}:{}", endpoint.address().to_string(), endpoint.port());
            handler(endpoint.address().to_string());
        }
#else
        Log::Error("http: TODO: Emscripten HTTP request");
        handler(std::unexpected(std::make_error_code(std::errc::not_supported)));
        co_return;
#endif
    }
};

int main()
{
    Log::Info(">>> starting");

    const asio::any_io_executor executor = asio::system_executor();

    auto TryHttp = [&executor](std::string_view url) {
        SimpleClient::Get(executor, url, [url](auto result) {
            if (result) {
                Log::InfoF("TryHttp: succeeded: '{}': {}", url, *result);
            } else {
                Log::ErrorF("TryHttp: failed: '{}': \"{}\"", url, result.error().message());
            }
        });
    };
    TryHttp("http://ifconfig.io");
    //TryHttp("https://httpbin.org/headers");

    asio::detail::global<asio::system_context>().join();

    Log::Info("<<< exiting");
    return 0;
}
