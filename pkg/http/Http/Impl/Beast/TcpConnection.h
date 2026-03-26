#pragma once
#if !__EMSCRIPTEN__
#include <boost/asio/ip/tcp.hpp>

namespace Http
{
    struct TcpConnection
    {
        using Socket = boost::asio::ip::tcp::socket;
        Socket socket;

        explicit TcpConnection(Socket&& socket_);
        ~TcpConnection();

        TcpConnection(const TcpConnection&) = delete;
        TcpConnection& operator=(const TcpConnection&) = delete;
        TcpConnection(TcpConnection&&) noexcept = default;
        TcpConnection& operator=(TcpConnection&&) noexcept = default;

        [[nodiscard]] static bool SocketConnected(Socket& socket);
        static void SocketShutdown(Socket& socket);
        static void SocketClose(Socket& socket, bool logSuccess);
        static void SocketFinish(Socket& socket);
    };
}
#endif
