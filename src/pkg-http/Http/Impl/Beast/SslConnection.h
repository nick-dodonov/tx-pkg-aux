#pragma once
#if !__EMSCRIPTEN__
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace Http
{
    struct SslConnection
    {
        using Socket = boost::asio::ip::tcp::socket;
        using Stream = boost::asio::ssl::stream<Socket>;
        Stream stream;

        explicit SslConnection(Stream&& stream_);
        ~SslConnection();

        SslConnection(const SslConnection&) = delete;
        SslConnection& operator=(const SslConnection&) = delete;
        SslConnection(SslConnection&&) noexcept = default;
        SslConnection& operator=(SslConnection&&) noexcept = default;

        bool IsHandshakeComplete();
    };
}
#endif
