//#include "Http/SimpleClient.h"
#include "Log/Log.h"
#include <asio.hpp>
#include <ada.h>
#include <expected>
#if __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <boost/beast.hpp>
#endif

using namespace std::chrono_literals;
namespace beast = boost::beast;
namespace http = beast::http;

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

        // URL parsing
        auto url_ec = ada::parse<ada::url_aggregator>(url);
        if (!url_ec) {
            Log::ErrorF("http: url parse failed: '{}'", url);
            handler(std::unexpected(std::make_error_code(std::errc::invalid_argument)));
            co_return;
        }

        auto& url_result = url_ec.value();
        Log::DebugF("http: url parsed protocol={} host={} port={} path={}", 
            url_result.get_protocol(),
            url_result.get_hostname(),
            url_result.get_port(),
            url_result.get_pathname()
        );

#if __EMSCRIPTEN__
        Log::Error("http: TODO: Emscripten HTTP request");
        handler(std::unexpected(std::make_error_code(std::errc::not_supported)));
        co_return;
#else
        auto executor = co_await asio::this_coro::executor;
        asio::ip::tcp::resolver resolver{executor};

        // DNS resolution
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
        asio::error_code ec;
        auto dns_results = co_await resolver.async_resolve(
            url_result.get_hostname(), 
            url_service, 
            asio::redirect_error(asio::use_awaitable, ec)
        );
        if (ec) {
            Log::ErrorF("http: failed to resolved: {}", ec.message());
            handler(std::unexpected(ec));
            co_return;
        }        
        for (const auto& entry : dns_results) {
            auto endpoint = entry.endpoint();
            Log::DebugF("http: resolved: {}:{}", endpoint.address().to_string(), endpoint.port());
        }

        // TCP connect
        asio::ip::tcp::socket socket{executor};
        
        for (const auto& entry : dns_results) {
            auto endpoint = entry.endpoint();
            std::tie(ec) = co_await socket.async_connect(
                endpoint,
                asio::as_tuple(asio::use_awaitable)
            );
            if (!ec) {
                Log::DebugF("http: connect succeed: {}:{}", endpoint.address().to_string(), endpoint.port());
                break;
            }
            Log::DebugF("http: connect failed: {}:{}: {}", endpoint.address().to_string(), endpoint.port(), ec.message());
            socket_close(socket, false);
        }

        if (ec) {
            Log::ErrorF("http: connect failed to any endpoint: {}", ec.message());
            socket_close(socket, false);
            handler(std::unexpected(ec));
            co_return;
        }

        //TODO: HTTP request/response
        // socket().set_option(boost::asio::ip::tcp::no_delay(true));
        // do_write();
        // do_read();
        http::request<beast::http::string_body> req{
            http::verb::get, 
            url_result.get_pathname(), 
            11};
        req.set(http::field::host, url_result.get_hostname());
        req.set(http::field::user_agent, "Test/1.0");

        // Send
        co_await http::async_write(socket, req, asio::use_awaitable);

        // Receive
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        
        std::tie(ec) = co_await http::async_read(
            socket, buffer, res, asio::as_tuple(asio::use_awaitable));

        //// TCP close
        //TODO: shutdown?
        socket_close(socket, true);

        //TODO: actual HTTP request/response handling
        handler(std::string("HTTP response data placeholder"));
#endif
    }

    static void socket_close(asio::ip::tcp::socket& socket, bool logSuccess)
    {
        asio::error_code ec;
        if (socket.close(ec)) {
            Log::WarnF("http: socket close failed: {}", ec.message());
        } else if (logSuccess) {
            Log::DebugF("http: socket closed");
        }
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
        Log::InfoF(">>> TryHttp: request: '{}'", url);
        SimpleClient::Get(executor, url, [url](auto result) {
            if (result) {
                Log::InfoF("<<< TryHttp: succeeded: '{}': {}", url, *result);
            } else {
                Log::ErrorF("<<< TryHttp: failed: '{}': \"{}\"", url, result.error().message());
            }
        });
    };

    //TryHttp("http://ifconfig.io");
    //TryHttp("https://httpbin.org/headers");

    TryHttp("http://url error/");
    TryHttp("http://localhost:12345/connect refused");
    //TryHttp("http://httpbun.com/status/200");

#if __EMSCRIPTEN__
    emscripten_set_main_loop_arg(
        [](void* arg) {
            auto io_context = static_cast<asio::io_context*>(arg);
            auto count = io_context->run_for(16ms);
            if (io_context->stopped()) {
                Log::DebugF("emscripten: ran count: {} (cancel)", count);
                emscripten_cancel_main_loop();
                Log::Debug("emscripten: exit(0)");
                exit(0);
                Log::Debug("emscripten: emscripten_force_exit(0)");
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
