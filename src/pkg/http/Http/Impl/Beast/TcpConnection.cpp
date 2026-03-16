#if !__EMSCRIPTEN__
#include "TcpConnection.h"
#include "Log/Log.h"

namespace Http
{
    TcpConnection::TcpConnection(Socket&& socket_)
        : socket(std::move(socket_))
    {}

    TcpConnection::~TcpConnection()
    {
        SocketFinish(socket);
    }

    [[nodiscard]] bool TcpConnection::SocketConnected(boost::asio::ip::tcp::socket& socket)
    {
        boost::system::error_code ec;
        socket.remote_endpoint(ec);
        if (ec) {
            return false;
        }

        // ioctl check (doesn't detect disconnection after ssl shutdown w/ boost::asio::ssl::error::stream_truncated error)
        socket.available(ec);
        if (ec) {
            return false;
        }

        // peed read in non_blocking mode detects disconnection state
        if (socket.non_blocking()) {
            char dummy{};
            socket.receive(
                boost::asio::buffer(&dummy, 1),
                boost::asio::socket_base::message_peek,
                ec
            );
            // ec == boost::asio::error::eof is equal to stream_truncated
            return !ec || ec == boost::asio::error::would_block;
        }

        return true;
    }

    void TcpConnection::SocketShutdown(Socket& socket)
    {
        boost::system::error_code ec;
        if (socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec)) {
            Log::Debug("http: socket: shutdown failed: {}", ec.message());
        } else {
            Log::Trace("http: socket: shutdown succeed");
        }
    }

    void TcpConnection::SocketClose(Socket& socket, const bool logSuccess)
    {
        boost::system::error_code ec;
        if (socket.close(ec)) {
            Log::Debug("http: socket: close failed: {}", ec.message());
        } else if (logSuccess) {
            Log::Trace("http: socket: close succeed");
        }
    }

    void TcpConnection::SocketFinish(Socket& socket)
    {
        if (!socket.is_open()) {
            return;
        }
        if (SocketConnected(socket)) {
            SocketShutdown(socket);
        }
        SocketClose(socket, true);
    }
}
#endif
