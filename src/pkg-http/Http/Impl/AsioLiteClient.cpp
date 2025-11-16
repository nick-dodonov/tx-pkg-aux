#if !__EMSCRIPTEN__
#include "AsioLiteClient.h"
#include "Log/Log.h"
#include <ada.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace Http
{
    // static boost::asio::awaitable<void> TestAsync()
    // {
    //     Log::Debug("111111111111");
    //     auto executor = co_await boost::asio::this_coro::executor;
    //     boost::asio::steady_timer timer{executor, std::chrono::seconds(1)};
    //     co_await timer.async_wait(boost::asio::use_awaitable);
    //     Log::Debug("222222222222");
    // }

    static void SocketClose(boost::asio::ip::tcp::socket& socket, bool logSuccess)
    {
        boost::system::error_code ec;
        if (socket.close(ec)) {
            Log::Warn("http: socket: close failed: {}", ec.what());
        } else if (logSuccess) {
            Log::Debug("http: socket: closed");
        }
    }

    boost::asio::awaitable<ILiteClient::Result> AsioLiteClient::GetAsync(std::string url)
    {
        Log::Debug("http: async: '{}'", url);
        //co_await TestAsync();

        // URL parsing
        auto url_ec = ada::parse<ada::url_aggregator>(url);
        if (!url_ec) {
            Log::Error("http: url parse failed: '{}'", url);
            co_return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        auto& url_result = url_ec.value();
        Log::Debug(
            "http: url parsed: protocol={} host={} port={} path={}",
            url_result.get_protocol(),
            url_result.get_hostname(),
            url_result.get_port(),
            url_result.get_pathname()
        );

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
            Log::Error("http: failed to resolved: {}", ec.what());
            co_return std::unexpected(ec);
        }        
        for (const auto& entry : dns_results) {
            auto endpoint = entry.endpoint();
            Log::Debug("http: resolved: {}:{}", endpoint.address().to_string(), endpoint.port());
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
                Log::Debug("http: socket: connected: {}:{}", endpoint.address().to_string(), endpoint.port());
                break;
            }
            Log::Warn("http: socket: connect failed: {}:{}: {}", endpoint.address().to_string(), endpoint.port(), ec.what());
            SocketClose(socket, false);
        }

        if (ec) {
            Log::Error("http: socket: connect failed to any endpoint: {}", ec.what());
            co_return std::unexpected(ec);
        }

        // TCP connected
        if (socket.set_option(boost::asio::ip::tcp::no_delay(true), ec)) {
            Log::Warn("http: socket: set_option TCP_NODELAY failed: {}", ec.what());
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
            Log::Error("http: sending failed: {} (count={})", ec.what(), count);
            SocketClose(socket, true);
            co_return std::unexpected(ec);
        }
        Log::Debug("http: sent: {} bytes", count);

        // Receive
        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        std::tie(ec, count) = co_await http::async_read(
            socket, 
            buffer, 
            response, 
            asio::as_tuple(asio::use_awaitable));
        if (ec) {
            Log::Error("http: receive failed: {} (count={})", ec.what(), count);
            SocketClose(socket, true);
            co_return std::unexpected(ec);
        }
        Log::Debug("http: received: {} bytes", count);

        // Convert Beast buffer/response to std::string and deliver result.
        std::string body = !response.body().empty()
            ? response.body()
            : beast::buffers_to_string(buffer.data());

        Log::Debug("http: response: {} ({}) body.size={}",
            response.result_int(), response.reason(), body.size());

        // TCP close
        //TODO: shutdown?
        SocketClose(socket, true);

        co_return body;        
    }
}
#endif
