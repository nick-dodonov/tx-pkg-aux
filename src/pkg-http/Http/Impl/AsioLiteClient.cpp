#include <string_view>
#if !__EMSCRIPTEN__
#include "AsioLiteClient.h"
#include "Log/Log.h"
#include "CstrView.h"
#include <format>
#include <array>
#include <ada.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/asio/ssl.hpp>

namespace Http
{
    template <typename Protocol, typename Executor>
    static void SocketClose(boost::asio::basic_socket<Protocol, Executor>& socket, bool logSuccess = true)
    {
        boost::system::error_code ec;
        if (socket.close(ec)) {
            Log::Warn("http: socket: close failed: {}", ec.message());
        } else if (logSuccess) {
            Log::Trace("http: socket: closed");
        }
    }

    static void LogTraceTls(SSL* ssl_handle)
    {
        // TLS/SSL version
        const char* tls_version = SSL_get_version(ssl_handle);
        Log::Trace("http: tls: protocol version: {}", tls_version);

        // Cipher being used
        const char* cipher_name = SSL_get_cipher_name(ssl_handle);
        const char* cipher_version = SSL_get_cipher_version(ssl_handle);
        int cipher_bits = SSL_get_cipher_bits(ssl_handle, nullptr);
        Log::Trace("http: tls: cipher: {} ({}, {} bits)", cipher_name, cipher_version, cipher_bits);
        const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_handle);
        if (cipher) {
            std::array<char, 256> cipher_desc{};
            SSL_CIPHER_description(cipher, cipher_desc.data(), cipher_desc.size());
            Log::Trace("http: tls: cipher details: {}", 
                std::string_view(cipher_desc.data(), std::strlen(cipher_desc.data())).substr(0, 
                    std::string_view(cipher_desc.data()).find_last_not_of(" \n\r\t") + 1));
        }

        // Session reused
        auto session_reused = SSL_session_reused(ssl_handle);
        Log::Trace("http: tls: session reused={}", session_reused);

        // Server certificate
        X509* cert = SSL_get_peer_certificate(ssl_handle);
        if (cert) {
            std::array<char, 256> subject{};
            std::array<char, 256> issuer{};
            X509_NAME_oneline(X509_get_subject_name(cert), subject.data(), subject.size());
            X509_NAME_oneline(X509_get_issuer_name(cert), issuer.data(), issuer.size());

            Log::Trace("http: tls: server cert subject: {}", subject.data());
            Log::Trace("http: tls: server cert issuer: {}", issuer.data());

            // Check certificate verification result
            long verify_result = SSL_get_verify_result(ssl_handle);
            if (verify_result == X509_V_OK) {
                Log::Trace("http: tls: certificate verification: OK");
            } else {
                Log::Trace("http: tls: certificate verification failed: {} ({})", 
                    X509_verify_cert_error_string(verify_result), verify_result);
            }

            X509_free(cert);
        } else {
            Log::Trace("http: tls: no server certificate");
        }

        // ALPN (Application-Layer Protocol Negotiation) for HTTP/2, HTTP/3
        const unsigned char* alpn_selected = nullptr;
        unsigned int alpn_len = 0;
        SSL_get0_alpn_selected(ssl_handle, &alpn_selected, &alpn_len);
        if (alpn_selected && alpn_len > 0) {
            std::string_view alpn_view{
                reinterpret_cast<const char*>(alpn_selected), //NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                alpn_len};
            Log::Trace("http: tls: ALPN negotiated: {}", alpn_view);
        } else {
            Log::Trace("http: tls: no ALPN negotiated (HTTP/1.1)");
        }
    }

    boost::asio::awaitable<ILiteClient::Result> AsioLiteClient::GetAsync(std::string url)
    {
        Log::Trace("http: async: {}", url);

        // URL parsing
        auto url_ec = ada::parse<ada::url_aggregator>(url);
        if (!url_ec) {
            Log::Error("http: url parse failed: {}", url);
            co_return std::unexpected(std::system_error{
                std::make_error_code(std::errc::invalid_argument), 
                std::format("Failed to parse URL: {}", url)
            });
        }

        auto& url_result = url_ec.value();
        Log::Trace(
            "http: url parsed: protocol={} host={} port={} path={}",
            url_result.get_protocol(),
            url_result.get_hostname(),
            url_result.get_port(),
            url_result.get_pathname()
        );

        namespace asio = boost::asio;
        namespace beast = boost::beast;
        namespace http = beast::http;
        namespace ssl = boost::asio::ssl;

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
            Log::Error("http: failed to resolved: {}", ec.message());
            co_return std::unexpected(std::system_error{
                ec, std::format("DNS resolution failed: '{}'", url_result.get_hostname())
            });
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
            Log::Warn("http: socket: connect failed: {}:{}: {}", endpoint.address().to_string(), endpoint.port(), ec.message());
            SocketClose(socket, false);
        }

        if (ec) {
            Log::Error("http: socket: connect failed to any endpoint: {}", ec.message());
            co_return std::unexpected(std::system_error{
                ec, std::format("Failed to connect to host: '{}'", url_result.get_hostname())
            });
        }

        // TCP connected
        if (socket.set_option(boost::asio::ip::tcp::no_delay(true), ec)) {
            Log::Warn("http: socket: set_option TCP_NODELAY failed: {}", ec.message());
        }

        http::response<http::string_body> response;
        std::string body;

        if (url_result.get_protocol() == "https:") {
            // TLS handshake
            Log::Trace("http: tls: handshaking");
            asio::ssl::context sslContext{asio::ssl::context::tls_client};
            sslContext.set_verify_mode(asio::ssl::verify_peer);
            sslContext.set_default_verify_paths();

            ssl::stream<asio::ip::tcp::socket> sslStream{std::move(socket), sslContext};
            String::CstrView cstrHostName{url_result.get_hostname()};
            if (!SSL_set_tlsext_host_name(sslStream.native_handle(), cstrHostName.c_str())) {
                Log::Error("http: tls: failed to set SNI");
                auto& underlying_socket = sslStream.lowest_layer();
                SocketClose(underlying_socket, true);
                co_return std::unexpected(std::system_error{
                    std::make_error_code(std::errc::protocol_error),
                    std::format("Failed to set SNI for: '{}'", url_result.get_hostname())
                });
            }

            std::tie(ec) = co_await sslStream.async_handshake(
                ssl::stream_base::client, 
                asio::as_tuple(asio::use_awaitable));
            if (ec) {
                Log::Error("http: tls: handshaking failed: {}", ec.message());

                // НЕ делаем async_shutdown - handshake не прошел
                // Просто закрываем underlying socket
                auto& underlying_socket = sslStream.lowest_layer();
                SocketClose(underlying_socket, true);

                co_return std::unexpected(std::system_error{ec, 
                    std::format("Failed TLS handshake: '{}'", url_result.get_hostname())
                });
            }

            Log::Debug("http: tls: handshake successful");
            if (Log::Enabled(Log::Level::Trace)) {
                LogTraceTls(sslStream.native_handle());
            }

            // Graceful SSL shutdown
            Log::Trace("http: tls: shutting down");
            std::tie(ec) = co_await sslStream.async_shutdown(asio::as_tuple(asio::use_awaitable));
            if (ec && ec != asio::error::eof) {
                // EOF is expected - server closed connection after shutdown
                Log::Warn("http: tls: shutdown warning: {}", ec.message());
            }

            // Closing underlying TCP socket
            auto& underlying_socket = sslStream.lowest_layer();
            SocketClose(underlying_socket, true);

            //XXXXXXXXXXXXXXXXXXXXXXXX
            co_return std::unexpected(std::system_error{
                std::make_error_code(std::errc::not_supported),
                std::format("XXXXXXXXXXXXXXXXXXXXXX: {}", url)
            });
        }
        else 
        {
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
                Log::Error("http: sending failed: {} (count={})", ec.message(), count);
                SocketClose(socket, true);
                co_return std::unexpected(std::system_error{
                    ec, std::format("Failed to send HTTP request: '{}' {} {}", 
                        url_result.get_hostname(), request.method_string(), request.target())
                });
            }
            Log::Debug("http: sent: {} bytes", count);

            // Receive
            beast::flat_buffer buffer;
            std::tie(ec, count) = co_await http::async_read(
                socket, 
                buffer, 
                response, 
                asio::as_tuple(asio::use_awaitable));
            // beast::string_view differs from std::string_view and isn't formatted well
            auto reason_view = std::string_view(response.reason().data(), response.reason().size());
            if (ec) {
                Log::Error("http: receive failed: {} (count={})", ec.message(), count);
                SocketClose(socket, true);
                co_return std::unexpected(std::system_error{
                    ec, 
                    std::format("Failed to receive HTTP response: {} ({})", 
                        response.result_int(), reason_view)
                });
            }
            Log::Debug("http: received: {} bytes", count);

            // Convert Beast buffer/response to std::string and deliver result.
            std::string body = !response.body().empty()
                ? response.body()
                : beast::buffers_to_string(buffer.data());

            Log::Debug("http: response: {} ({}) body.size={}",
                response.result_int(), reason_view, body.size());
        }

        // TCP close
        //TODO: shutdown?
        SocketClose(socket, true);

        co_return Response {
            .statusCode = static_cast<int>(response.result_int()),
            .body = std::move(body),
        };
    }
}
#endif
