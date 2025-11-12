//#include "Http/SimpleClient.h"
#include "Log/Log.h"
#include <asio.hpp>
#include <ada.h>
#include <expected>
#include <functional>
#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace std::chrono_literals;

class SimpleClient {
public:
    using Result = std::expected<std::string, std::error_code>;
    using Callback = std::function<void(Result result)>;

    static void Get(
        const asio::any_io_executor& executor,
        const std::string_view url,
        const Callback& handler)
    {
        asio::co_spawn(
            executor, 
            GetAsync(std::string{url}, handler), 
            //asio::detached
            [handler](const std::exception_ptr& e) {
                if (e) {
                    Log::ErrorF("http: exception in co_spawn");
                    handler(std::unexpected(std::make_error_code(std::errc::operation_canceled)));
                }            
            }
        );
    }

private:
    static asio::awaitable<void> GetAsync(
        std::string url,
        Callback handler)
    {
        Log::DebugF("http: query: '{}'", url);
        //auto executor = asio::get_associated_executor(asio::use_awaitable);
        auto executor = co_await asio::this_coro::executor;
        asio::ip::tcp::resolver resolver{executor};

        // asio::error_code ec;
        // auto results = resolver.resolve("exam ple.com", {}, ec);
        // if (ec) {
        //     Log::ErrorF("http: failed to parse: {}", ec.message());
        //     co_return;
        // }
        // for (const auto& entry : results) {
        //     auto endpoint = entry.endpoint();
        //     Log::DebugF("http: resolved: {}:{}", endpoint.address().to_string(), endpoint.port());
        //     handler(endpoint.address().to_string());
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
        //
        // DNS resolution
        //
        auto url_protocol = url_result.get_protocol();
        auto url_port = url_result.get_port();
        std::string url_service;
        if (!url_port.empty()) {
            url_service = url_port;
        } else if (!url_protocol.empty()) {
            url_service = url_protocol.substr(0, url_protocol.size()-1); // resolver supports "http" and "https" services
        } else {
            url_service = "80"; // default
        }
        asio::error_code error_code;
        auto dns_results = co_await resolver.async_resolve(
            url_result.get_hostname(), 
            url_service, 
            asio::redirect_error(asio::use_awaitable, error_code)
        );
        if (error_code) {
            Log::ErrorF("http: failed to resolved: {}", error_code.message());
            co_return;
        }        
        for (const auto& entry : dns_results) {
            auto endpoint = entry.endpoint();
            Log::DebugF("http: resolved: {}:{}", endpoint.address().to_string(), endpoint.port());
            handler(endpoint.address().to_string());
        }

        //
        // TCP connect
        //
        asio::ip::tcp::socket socket{executor};
        
        for (const auto& entry : dns_results) {
            auto endpoint = entry.endpoint();
            std::tie(error_code) = co_await socket.async_connect(
                endpoint,
                asio::as_tuple(asio::use_awaitable)
            );
            if (!error_code) {
                Log::DebugF("http: connected to: {}:{}", endpoint.address().to_string(), endpoint.port());
                break;
            }
            Log::DebugF("http: failed to connect to: {}:{}: {}", endpoint.address().to_string(), endpoint.port(), error_code.message());
        }

        if (error_code) {
            Log::ErrorF("http: failed to connect to any endpoint: {}", error_code.message());
            socket.close();
            co_return;
        }

        //TODO: 
        // socket().set_option(boost::asio::ip::tcp::no_delay(true));
        // do_write();
        // do_read();

        //
        // TCP close
        //
        //TODO: shutdown?
        socket.close();
        Log::DebugF("http: socket closed");

#else
        Log::Error("http: TODO: Emscripten HTTP request");
        handler(std::unexpected(std::make_error_code(std::errc::not_supported)));
        co_return;
#endif
    }
};

static asio::io_context& get_io_context()
{
    static asio::io_context ctx;
    return ctx;
}

int main()
{
    Log::Info(">>> starting");

    //auto executor = asio::system_executor();
    auto& io_context = get_io_context();
    auto executor = io_context.get_executor();

    auto TryHttp = [&executor](std::string_view url) {
        SimpleClient::Get(executor, url, [url](auto result) {
            if (result) {
                Log::InfoF("TryHttp: succeeded: '{}': {}", url, *result);
            } else {
                Log::ErrorF("TryHttp: failed: '{}': \"{}\"", url, result.error().message());
            }
        });
    };

    //TryHttp("http://ifconfig.io");
    //TryHttp("https://httpbin.org/headers");

    TryHttp("http://httpbun.com/status/200");
    //TryHttp("https://httpbun.com/status/200");

#if __EMSCRIPTEN__
    emscripten_set_main_loop_arg(
        [](void* arg) {
            auto io_context = static_cast<asio::io_context*>(arg);
            auto count = io_context->run_for(16ms);
            if (io_context->stopped()) {
                Log::DebugF("emscripten: ran count: {} (cancel)", count);
                emscripten_cancel_main_loop();
                Log::Debug("emscripten: exit()");
                exit(0);
                Log::Debug("emscripten: emscripten_force_exit()");
                emscripten_force_exit(0);
            }
            else if (count > 0) {
                Log::DebugF("emscripten: ran count: {} (continue)", count);
            }
        },
        &io_context,
        0,
        1
    );
#else
    //asio::detail::global<asio::system_context>().join();
    io_context.run();
#endif

    Log::Info("<<< exiting");
    return 0;
}
