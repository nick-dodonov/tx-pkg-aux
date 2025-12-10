#if !__EMSCRIPTEN__
#include "SslConnection.h"
#include "TcpConnection.h"

#include "Log/Log.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/detached.hpp>

namespace Http
{
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
