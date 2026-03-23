#if !__EMSCRIPTEN__ && defined(HTTP_CLIENT_WITH_SSL)
#include "SslConnection.h"
#include "TcpConnection.h"

#include "Log/Log.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/detached.hpp>

namespace Http
{
    void LogSslConnected(const SSL* ssl_handle)
    {
        if (!Log::Enabled(Log::Level::Trace)) {
            return;
        }

        // TLS/SSL version
        const auto* tls_version = SSL_get_version(ssl_handle);
        Log::Trace("http: tls: handshake succeed: {}", tls_version);

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
                    X509_verify_cert_error_string(verify_result),
                    verify_result);
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

    SslConnection::SslConnection(Stream&& stream_)
        : stream(std::move(stream_))
    {}

    SslConnection::~SslConnection()
    {
        if (!IsHandshakeComplete()) {
            TcpConnection::SocketFinish(stream.next_layer());
            return;
        }

        // Graceful SSL shutdown
        Log::Trace("http: tls: shutting down");
        auto StreamFinish = [](Stream stream) -> boost::asio::awaitable<void> {
            auto [ec] = co_await stream.async_shutdown(
                boost::asio::as_tuple(boost::asio::use_awaitable));
            if (ec/* && ec != asio::error::eof*/) {
                // EOF is expected - server closed connection after shutdown
                // boost::asio::ssl::error::stream_truncated - The underlying stream closed before the ssl stream gracefully shut down.
                Log::Trace("http: tls: shutdown: {}", ec.message());
            } else {
                Log::Trace("http: tls: shutdown succeed"); //, ec ? ec.message(): "succeed");
            }

            TcpConnection::SocketFinish(stream.next_layer());
        };
        const auto executor = stream.get_executor(); // before moving stream
        boost::asio::co_spawn(
            executor,
            StreamFinish(std::move(stream)),
            boost::asio::detached
        );
    }

    bool SslConnection::IsHandshakeComplete()
    {
        if (const auto* sslHandle = stream.native_handle()) {
            return SSL_is_init_finished(sslHandle) != 0;
        }
        return false;
    }
}
#endif
