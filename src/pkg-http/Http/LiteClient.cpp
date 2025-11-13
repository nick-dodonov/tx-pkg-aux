#include "LiteClient.h"
#include "Log/Log.h"
#include <ada.h>
#include <boost/asio.hpp>
#if !__EMSCRIPTEN__
    #include <boost/beast.hpp>
#endif

namespace Http {
#if !__EMSCRIPTEN__
    static void socket_close(boost::asio::ip::tcp::socket& socket, bool logSuccess)
    {
        boost::system::error_code ec;
        if (socket.close(ec)) {
            Log::WarnF("http: socket close failed: {}", ec.what());
        } else if (logSuccess) {
            Log::DebugF("http: socket closed");
        }
    }
#endif

    static boost::asio::awaitable<ILiteClient::Result> GetAsync(std::string url)
    {
        Log::DebugF("http: query: '{}'", url);

        // URL parsing
        auto url_ec = ada::parse<ada::url_aggregator>(url);
        if (!url_ec) {
            Log::ErrorF("http: url parse failed: '{}'", url);
            co_return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        auto& url_result = url_ec.value();
        Log::DebugF(
            "http: url parsed: protocol={} host={} port={} path={}",
            url_result.get_protocol(),
            url_result.get_hostname(),
            url_result.get_port(),
            url_result.get_pathname()
        );

#if __EMSCRIPTEN__
        Log::Error("http: TODO: Emscripten HTTP request");
        co_return std::unexpected(std::make_error_code(std::errc::not_supported));
#else
        namespace asio = boost::asio;
        namespace beast = boost::beast;
        namespace http = beast::http;

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
        boost::system::error_code ec;
        auto dns_results = co_await resolver.async_resolve(
            url_result.get_hostname(), 
            url_service, 
            asio::redirect_error(asio::use_awaitable, ec)
        );
        if (ec) {
            Log::ErrorF("http: failed to resolved: {}", ec.what());
            co_return std::unexpected(ec);
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
            Log::WarnF("http: connect failed: {}:{}: {}", endpoint.address().to_string(), endpoint.port(), ec.what());
            socket_close(socket, false);
        }

        if (ec) {
            Log::ErrorF("http: connect failed to any endpoint: {}", ec.what());
            co_return std::unexpected(ec);
        }

        // TCP connected
        if (socket.set_option(boost::asio::ip::tcp::no_delay(true), ec)) {
            Log::WarnF("http: set_option TCP_NODELAY failed: {}", ec.what());
        }

        // HTTP request/response
        http::request<beast::http::string_body> request{
            http::verb::get, 
            url_result.get_pathname(), 
            11};
        request.set(http::field::host, url_result.get_hostname());
        request.set(http::field::user_agent, "Test/1.0");

        // Send
        unsigned long count = 0;
        std::tie(ec, count) = co_await http::async_write(
            socket, 
            request, 
            asio::as_tuple(asio::use_awaitable));
        if (ec) {
            Log::ErrorF("http: sending failed: {} (count={})", ec.what(), count);
            socket_close(socket, true);
            co_return std::unexpected(ec);
        }
        Log::DebugF("http: sent: {} bytes", count);

        // Receive
        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        std::tie(ec, count) = co_await http::async_read(
            socket, 
            buffer, 
            response, 
            asio::as_tuple(asio::use_awaitable));
        if (ec) {
            Log::ErrorF("http: receive failed: {} (count={})", ec.what(), count);
            socket_close(socket, true);
            co_return std::unexpected(ec);
        }
        Log::DebugF("http: received: {} bytes", count);

        // Convert Beast buffer/response to std::string and deliver result.
        std::string body = !response.body().empty()
            ? response.body()
            : beast::buffers_to_string(buffer.data());

        Log::DebugF("http: response: {} ({}) body.size={}",
            response.result_int(), response.reason(), body.size());

        // TCP close
        //TODO: shutdown?
        socket_close(socket, true);

        co_return body;
#endif
    }

    LiteClient::LiteClient(boost::asio::any_io_executor executor)
        : _executor(std::move(executor))
    {}

    void LiteClient::Get(const std::string_view url, Callback&& handler)
    {
        boost::asio::co_spawn(
            _executor,
            GetAsync(std::string{url}),
            // asio::detached
            [handler = std::move(handler)](const std::exception_ptr& e, Result&& result) {
                if (e) {
                    Log::ErrorF("http: co_spawn exception");
                    handler(std::unexpected(std::make_error_code(std::errc::operation_canceled)));
                } else {
                    // Log::DebugF("http: co_spawn succeed");
                    handler(std::move(result));
                }
            }
        );
    }
}
