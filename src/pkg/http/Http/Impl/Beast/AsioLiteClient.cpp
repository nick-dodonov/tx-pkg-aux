#if !__EMSCRIPTEN__
#include "AsioLiteClient.h"
#include "Log/Log.h"
#include "TcpConnection.h"

#include <ada.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

#if defined(HTTP_CLIENT_WITH_SSL)
#include "SslConnection.h"
#include "../CstrView.h"
#include <boost/asio/ssl.hpp>
#endif

namespace Http
{
    auto constexpr TlsUrlProtocol = "https:";
    auto constexpr BasicHttpVersion = 11;
    auto constexpr DefaultRequestService = "80";

    using BasicSocket = boost::asio::basic_socket<boost::asio::ip::tcp>;

    [[nodiscard]] static BasicSocket::endpoint_type GetLocalEndpoint(const BasicSocket& socket)
    {
        boost::system::error_code ec;
        const auto endpoint = socket.local_endpoint(ec);
        if (ec) {
            Log::Debug("http: socket: local_endpoint failed: {}", ec.message());
        }
        return endpoint;
    }

    [[nodiscard]] static BasicSocket::endpoint_type GetRemoteEndpoint(const BasicSocket& socket)
    {
        boost::system::error_code ec;
        const auto endpoint = socket.remote_endpoint(ec);
        if (ec) {
            Log::Debug("http: socket: remote_endpoint failed: {}", ec.message());
        }
        return endpoint;
    }

    static void LogSocketConnected(const BasicSocket& socket)
    {
        if (Log::Enabled(Log::Level::Debug)) {
            const auto local = GetLocalEndpoint(socket);
            const auto remote = GetRemoteEndpoint(socket);
            Log::Debug("http: socket: connected: {}:{} -> {}:{}",
                local.address().to_string(), local.port(),
                remote.address().to_string(), remote.port());
        }
    }

    static void LogDnsResults(const boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>& dns_results)
    {
        if (Log::Enabled(Log::Level::Trace)) {
            for (const auto& entry : dns_results) {
                auto endpoint = entry.endpoint();
                Log::Trace("http: resolved: {}:{}", endpoint.address().to_string(), endpoint.port());
            }
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

        boost::asio::awaitable<ILiteClient::Result> GetAsyncImpl()
        {
            Log::Trace("http: coro: {}", _url);

            // URL parsing
            if (auto ec = UrlParse()) {
                Log::Debug("http: url parse failed: {}", _url);
                co_return std::unexpected(std::system_error{ec, std::format("URL parse failed: {}", _url)});
            }

            // DNS resolution
            if (auto ec = co_await DnsResolve()) {
                co_return std::unexpected(std::system_error{
                    ec, std::format("DNS resolve failed: '{}'", _url_result.get_hostname())
                });
            }

            // TCP connect
            auto tcpSocket = co_await TcpConnect();
            if (!tcpSocket) {
                co_return std::unexpected(std::system_error{
                    tcpSocket.error(), std::format("TCP connect failed: '{}'", _url_result.get_hostname())
                });
            }
            auto tcpConnection = TcpConnection{std::move(tcpSocket).value()};

            // TLS or plain HTTP handling
            if (_url_result.get_protocol() == TlsUrlProtocol) {
#if defined(HTTP_CLIENT_WITH_SSL)
                auto resultSslConnection = co_await SslConnect(std::move(tcpConnection));
                if (!resultSslConnection) {
                    co_return std::unexpected(std::move(resultSslConnection).error());
                }

                // Make HTTP request over TLS
                auto response = co_await MakeHttpRequest(resultSslConnection.value().stream);
                // TLS shutdown and TCP socket will be closed by scope
                co_return response;
#else
                co_return std::unexpected(std::system_error{
                    std::make_error_code(std::errc::protocol_not_supported),
                    "TLS requested but HTTP client is built without SSL support"
                });
#endif
            }

            // Make HTTP request over plain TCP
            auto response = co_await MakeHttpRequest(tcpConnection.socket);
            // TCP socket will be closed by scope
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
                url_service = url_protocol.substr(0, url_protocol.size() - 1); // resolver supports "http" and "https" services
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

        boost::asio::awaitable<std::expected<TcpConnection::Socket, boost::system::error_code>> TcpConnect() const
        {
            // TCP connect
            namespace asio = boost::asio;
            const auto executor = co_await asio::this_coro::executor;
            TcpConnection::Socket socket{executor};

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
                TcpConnection::SocketClose(socket, false);
            }

            if (ec) {
                Log::Debug("http: socket: connect failed to any endpoint: {}", ec.message());
                co_return std::unexpected(ec);
            }

            LogSocketConnected(socket);

            // Setup TCP_NODELAY
            if (socket.set_option(asio::ip::tcp::no_delay(true), ec)) {
                Log::Debug("http: socket: set_option TCP_NODELAY failed: {}", ec.message());
            }

            // Setup non-blocking mode
            // - We use always async operations, so it doesn't make sense to have blocking socket.
            // - It's useful to use peek `receive` w/ possible `would_block` check to get connection state immediately.
            if (socket.non_blocking(true, ec)) {
                Log::Debug("http: socket: native_non_blocking failed: {}", ec.message());
            }

            co_return socket;
        }

#if defined(HTTP_CLIENT_WITH_SSL)
        boost::asio::awaitable<std::expected<SslConnection, std::system_error>> SslConnect(TcpConnection&& tcpConnection) const // NOLINT(*-avoid-reference-coroutine-parameters)
        {
            namespace asio = boost::asio;
            namespace ssl = asio::ssl;

            // TLS connect
            const auto executor = co_await asio::this_coro::executor;
            ssl::context sslContext{ssl::context::tls_client};
            sslContext.set_verify_mode(ssl::verify_none); //TODO: rollback to ssl::verify_peer and add certs handling setup for development
            sslContext.set_default_verify_paths();

            auto sslStream = SslConnection::Stream{std::move(tcpConnection).socket, sslContext};
            auto sslConnection = SslConnection{std::move(sslStream)};

            // SNI setup
            String::CstrView cstrHostName{_url_result.get_hostname()};
            if (!SSL_set_tlsext_host_name(sslConnection.stream.native_handle(), cstrHostName.c_str())) {
                auto what = std::format("SNI setup failed '{}'", _url_result.get_hostname());
                Log::Debug("http: tls: {}", what);
                co_return std::unexpected(std::system_error{std::make_error_code(std::errc::protocol_error), what});
            }

            // TLS handshake
            auto [ec] = co_await sslConnection.stream.async_handshake(
                ssl::stream_base::client,
                asio::as_tuple(asio::use_awaitable));
            if (ec) {
                auto what = std::format("TLS handshake failed '{}:{}'", _url_result.get_hostname(), _url_result.get_port());
                Log::Debug("http: tls: {}: {}", what, ec.message());
                // Don't make async_shutdown - handshake didn't pass, just close lowest_layer() socket
                co_return std::unexpected(std::system_error{ec, what});
            }

            LogSslConnected(sslConnection.stream.native_handle());
            co_return std::move(sslConnection);
        }
#endif

        template <typename Connection>
        boost::asio::awaitable<ILiteClient::Result> MakeHttpRequest(Connection& connection) // NOLINT(*-avoid-reference-coroutine-parameters)
        {
            namespace asio = boost::asio;
            namespace beast = boost::beast;
            namespace http = beast::http;

            auto& stream = connection;

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
                        _url_result.get_hostname(),
                        request.method_string(),
                        request.target())
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
                        response.result_int(),
                        reason_view)
                });
            }
            Log::Trace("http: received: {} bytes", count);

            // Convert Beast buffer/response to std::string and deliver a result.
            auto body = !response.body().empty()
                            ? response.body()
                            : beast::buffers_to_string(buffer.data());

            Log::Trace("http: response: {} ({}) body.size={}",
                response.result_int(),
                reason_view,
                body.size());

            co_return ILiteClient::Response{
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
