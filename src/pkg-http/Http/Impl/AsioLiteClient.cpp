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
#include <boost/noncopyable.hpp>

namespace Http
{
    auto constexpr TlsUrlProtocol = "https:";
    auto constexpr BasicHttpVersion = 11;
    auto constexpr DefaultRequestService = "80";

    static void SocketClose(boost::asio::basic_socket<boost::asio::ip::tcp>& socket, const bool logSuccess)
    {
        boost::system::error_code ec;
        if (socket.close(ec)) {
            Log::Debug("http: socket: close failed: {}", ec.message());
        } else if (logSuccess) {
            Log::Trace("http: socket: close succeed");
        }
    }

    struct SocketScope : boost::noncopyable // NOLINT(*-special-member-functions)
    {
        using BasicSocket = boost::asio::basic_socket<boost::asio::ip::tcp>;
        BasicSocket& socket;

        explicit SocketScope(BasicSocket& socket_)
            : socket(socket_)
        {}

        ~SocketScope()
        {
            if (!socket.is_open()) {
                return;
            }
            if (IsConnected()) {
                if (boost::system::error_code ec; socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec)) {
                    Log::Trace("http: socket: shutdown: {}", ec.message());
                } else {
                    Log::Trace("http: socket: shutdown: succeed");
                }
            }
            SocketClose(socket, true);
        }

        [[nodiscard]] BasicSocket::endpoint_type GetLocalEndpoint() const
        {
            boost::system::error_code ec;
            const auto endpoint = socket.local_endpoint(ec);
            if (ec) {
                Log::Debug("http: socket: local_endpoint failed: {}", ec.message());
            }
            return endpoint;
        }

        [[nodiscard]] BasicSocket::endpoint_type GetRemoteEndpoint() const
        {
            boost::system::error_code ec;
            const auto endpoint = socket.remote_endpoint(ec);
            if (ec) {
                Log::Debug("http: socket: remote_endpoint failed: {}", ec.message());
            }
            return endpoint;
        }

        [[nodiscard]] bool IsConnected() const
        {
            boost::system::error_code ec;
            socket.remote_endpoint(ec);
            return !ec;
        }

        void LogConnected() const
        {
            if (Log::Enabled(Log::Level::Debug)) {
                const auto local = GetLocalEndpoint();
                const auto remote = GetRemoteEndpoint();
                Log::Debug("http: socket: connected: {}:{} -> {}:{}",
                    local.address().to_string(), local.port(),
                    remote.address().to_string(), remote.port());
            }
        }
    };

    static void LogDnsResults(const boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>& dns_results)
    {
        if (Log::Enabled(Log::Level::Trace)) {
            for (const auto& entry : dns_results) {
                auto endpoint = entry.endpoint();
                Log::Trace("http: resolved: {}:{}", endpoint.address().to_string(), endpoint.port());
            }
        }
    }

    static void LogSslHandle(const SSL* ssl_handle)
    {
        if (!Log::Enabled(Log::Level::Trace)) {
            return;
        }

        // TLS/SSL version
        const auto* tls_version = SSL_get_version(ssl_handle);
        Log::Trace("http: tls: protocol version: {}", tls_version);

        // Cipher being used
        const auto* cipher_name = SSL_get_cipher_name(ssl_handle);
        const auto* cipher_version = SSL_get_cipher_version(ssl_handle);
        auto cipher_bits = SSL_get_cipher_bits(ssl_handle, nullptr);
        Log::Trace("http: tls: cipher: {} ({}, {} bits)", cipher_name, cipher_version, cipher_bits);
        if (const auto* cipher = SSL_get_current_cipher(ssl_handle)) {
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
        if (auto* cert = SSL_get_peer_certificate(ssl_handle)) {
            std::array<char, 256> subject{};
            std::array<char, 256> issuer{};
            X509_NAME_oneline(X509_get_subject_name(cert), subject.data(), subject.size());
            X509_NAME_oneline(X509_get_issuer_name(cert), issuer.data(), issuer.size());

            Log::Trace("http: tls: server cert subject: {}", subject.data());
            Log::Trace("http: tls: server cert issuer: {}", issuer.data());

            // Check certificate verification result
            auto verify_result = SSL_get_verify_result(ssl_handle);
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

    class BeastRequestContext
    {
    public:
        explicit BeastRequestContext(std::string url)
            : _url(std::move(url))
        {}

        template <typename CompletionToken>
        auto GetAsync(CompletionToken&& token) // NOLINT(cppcoreguidelines-missing-std-forward)
        {
            namespace asio = boost::asio;
            return asio::async_initiate<CompletionToken, void(ILiteClient::Result)>(
                [&]<typename T0>(T0&& handler) {
                    auto executor = asio::get_associated_executor(handler);
                    auto slot = asio::get_associated_cancellation_slot(handler);
                    asio::co_spawn(
                        executor,
                        GetAsyncImpl(),
                        asio::bind_cancellation_slot(
                            slot,
                            [handler = std::forward<T0>(handler)](const std::exception_ptr& ex, ILiteClient::Result&& result) mutable {
                                if (!ex) {
                                    std::move(handler)(std::move(result));
                                } else {
                                    std::move(handler)(std::unexpected(
                                        std::system_error{
                                            std::make_error_code(std::errc::io_error),
                                            "Unknown error in co_spawn"
                                        }
                                    ));
                                }
                            })
                    );
                },
                token
            );
        }

    private:
        std::string _url;
        ada::url_aggregator _url_result;

        using DnsResults = boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>;
        DnsResults _dns_results;

        using Socket = boost::asio::ip::tcp::socket;
        using TlsStream = boost::asio::ssl::stream<Socket>;
        using Stream = std::variant<std::monostate, Socket, TlsStream>;
        Stream _stream;

        boost::asio::awaitable<ILiteClient::Result> GetAsyncImpl()
        {
            Log::Trace("http: coro: {}", _url);

            // URL parsing
            if (auto ec = UrlParse()) {
                Log::Debug("http: url parse failed: {}", _url);
                co_return std::unexpected(std::system_error{ec,std::format("URL parse failed: {}", _url)});
            }

            // DNS resolution
            if (auto ec = co_await DnsResolve()) {
                co_return std::unexpected(std::system_error{
                    ec, std::format("DNS resolve failed: '{}'", _url_result.get_hostname())
                });
            }

            namespace asio = boost::asio;
            namespace ssl = asio::ssl;

            // TCP connect
            if (auto ec = co_await TcpConnect()) {
                co_return std::unexpected(std::system_error{
                    ec, std::format("TCP connect failed: '{}'", _url_result.get_hostname())
                });
            }

            // Closeable socket wrapper
            auto& socket = *std::get_if<Socket>(&_stream);
            SocketScope tcpSocketScope{socket};
            tcpSocketScope.LogConnected();

            // TCP connected
            boost::system::error_code ec;
            if (socket.set_option(asio::ip::tcp::no_delay(true), ec)) {
                Log::Debug("http: socket: set_option TCP_NODELAY failed: {}", ec.message());
            }

            if (_url_result.get_protocol() == TlsUrlProtocol) {
                // TLS handshake
                Log::Trace("http: tls: handshaking");
                ssl::context sslContext{ssl::context::tls_client};
                sslContext.set_verify_mode(ssl::verify_peer);
                sslContext.set_default_verify_paths();

                TlsStream sslStream{std::move(socket), sslContext};
                SocketScope tlsSocketScope{sslStream.lowest_layer()};

                String::CstrView cstrHostName{_url_result.get_hostname()};
                if (!SSL_set_tlsext_host_name(sslStream.native_handle(), cstrHostName.c_str())) {
                    Log::Debug("http: tls: failed to set SNI");
                    co_return std::unexpected(std::system_error{
                        std::make_error_code(std::errc::protocol_error),
                        std::format("Failed to set SNI for: '{}'", _url_result.get_hostname())
                    });
                }

                std::tie(ec) = co_await sslStream.async_handshake(
                    ssl::stream_base::client,
                    asio::as_tuple(asio::use_awaitable));
                if (ec) {
                    Log::Debug("http: tls: handshaking failed: {}", ec.message());
                    // Don't make async_shutdown - handshake didn't pass, just close lowest_layer() socket
                    co_return std::unexpected(std::system_error{ec,
                        std::format("Failed TLS handshake: '{}'", _url_result.get_hostname())
                    });
                }

                Log::Trace("http: tls: handshake succeed");
                LogSslHandle(sslStream.native_handle());

                // Make HTTP request over TLS
                auto response = co_await MakeHttpRequest(sslStream);

                // Graceful SSL shutdown
                //Log::Trace("http: tls: shutting down");
                std::tie(ec) = co_await sslStream.async_shutdown(asio::as_tuple(asio::use_awaitable));
                if (ec && ec != asio::error::eof) { // EOF is expected - server closed connection after shutdown
                    Log::Debug("http: tls: shutdown failed: {}", ec.message());
                } else {
                    Log::Trace("http: tls: shutdown {}", ec ? ec.message(): "succeed");
                }

                // Underlying TCP socket will be closed by socket scope
                co_return response;
            }

            // Make HTTP request over plain TCP
            auto response = co_await MakeHttpRequest(socket);

            // TCP socket will be closed by socket scope
            co_return response;
        }

        std::error_code UrlParse()
        {
            // URL parsing
            auto url_ec = ada::parse<ada::url_aggregator>(_url);
            if (!url_ec) {
                return std::make_error_code(std::errc::invalid_argument);
            }
            _url_result = url_ec.value();
            Log::Trace(
                "http: url parsed: protocol={} host={} port={} path={}",
                _url_result.get_protocol(),
                _url_result.get_hostname(),
                _url_result.get_port(),
                _url_result.get_pathname()
            );
            return {};
        }

        boost::asio::awaitable<boost::system::error_code> DnsResolve()
        {
            namespace asio = boost::asio;

            // DNS resolution
            const auto executor = co_await asio::this_coro::executor;
            asio::ip::tcp::resolver resolver{executor};

            const auto url_protocol = _url_result.get_protocol();
            const auto url_port = _url_result.get_port();
            std::string url_service;
            if (!url_port.empty()) {
                url_service = url_port;
            } else if (!url_protocol.empty()) {
                url_service = url_protocol.substr(0, url_protocol.size()-1); // resolver supports "http" and "https" services
            } else {
                url_service = DefaultRequestService;
            }

            boost::system::error_code ec;
            _dns_results = co_await resolver.async_resolve(
                _url_result.get_hostname(),
                url_service,
                // https://think-async.com/Asio/asio-1.36.0/doc/asio/overview/composition/token_adapters.html
                asio::redirect_error(asio::use_awaitable, ec)
            );
            if (ec) {
                Log::Error("http: failed to resolved: {}", ec.message());
            } else {
                LogDnsResults(_dns_results);
            }
            co_return ec;
        }

        boost::asio::awaitable<boost::system::error_code> TcpConnect()
        {
            // TCP connect
            namespace asio = boost::asio;
            const auto executor = co_await asio::this_coro::executor;
            Socket socket{executor};

            boost::system::error_code ec;
            for (const auto& entry : _dns_results) {
                auto endpoint = entry.endpoint();
                std::tie(ec) = co_await socket.async_connect(
                    endpoint,
                    asio::as_tuple(asio::use_awaitable)
                );
                if (!ec) {
                    break;
                }
                Log::Trace("http: socket: connect failed: {}:{}: {}", endpoint.address().to_string(), endpoint.port(), ec.message());
                SocketClose(socket, false);
            }

            if (ec) {
                Log::Debug("http: socket: connect failed to any endpoint: {}", ec.message());
            }

            _stream.emplace<Socket>(std::move(socket));
            co_return ec;
        }

        template <typename Stream>
        boost::asio::awaitable<ILiteClient::Result> MakeHttpRequest(Stream& stream) // NOLINT(*-avoid-reference-coroutine-parameters)
        {
            namespace asio = boost::asio;
            namespace beast = boost::beast;
            namespace http = beast::http;

            // HTTP request/response
            http::request<http::string_body> request{
                http::verb::get,
                _url_result.get_pathname(),
                BasicHttpVersion};
            request.set(http::field::host, _url_result.get_hostname());
            request.set(http::field::user_agent, "Test/1.0");

            // Send
            auto [ec, count] = co_await http::async_write(
                stream,
                request,
                asio::as_tuple(asio::use_awaitable));
            if (ec) {
                Log::Debug("http: sending failed: {} (count={})", ec.message(), count);
                co_return std::unexpected(std::system_error{
                    ec, std::format("Failed to send HTTP request: '{}' {} {}",
                        _url_result.get_hostname(), request.method_string(), request.target())
                });
            }
            Log::Trace("http: sent: {} bytes", count);

            // Receive
            http::response<http::string_body> response;
            beast::flat_buffer buffer;
            std::tie(ec, count) = co_await http::async_read(
                stream,
                buffer,
                response,
                asio::as_tuple(asio::use_awaitable));

            // beast::string_view differs from std::string_view and isn't formatted well
            auto reason_view = std::string_view(response.reason().data(), response.reason().size());
            if (ec) {
                Log::Debug("http: receive failed: {} (count={})", ec.message(), count);
                co_return std::unexpected(std::system_error{
                    ec,
                    std::format("Failed to receive HTTP response: {} ({})",
                        response.result_int(), reason_view)
                });
            }
            Log::Trace("http: received: {} bytes", count);

            // Convert Beast buffer/response to std::string and deliver a result.
            auto body = !response.body().empty()
                ? response.body()
                : beast::buffers_to_string(buffer.data());

            Log::Trace("http: response: {} ({}) body.size={}",
                response.result_int(), reason_view, body.size());

            co_return ILiteClient::Response {
                .statusCode = static_cast<int>(response.result_int()),
                .body = std::move(body),
            };
        }
    };

    boost::asio::awaitable<ILiteClient::Result> AsioLiteClient::GetAsync(std::string url)
    {
        Log::Trace("http: async: {}", url);
        BeastRequestContext ctx{std::move(url)};
        auto result = co_await ctx.GetAsync(boost::asio::use_awaitable);
        co_return result;
    }
}
#endif
