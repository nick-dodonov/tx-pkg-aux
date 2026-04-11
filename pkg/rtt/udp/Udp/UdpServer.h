#pragma once
#include "Rtt/Transport.h"

#include <boost/asio/any_io_executor.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace Rtt::Udp
{
    /// Server-side UDP transport.
    ///
    /// Binds a local port and creates a new virtual link for each unique
    /// remote endpoint that sends a datagram. Open() may fire the
    /// acceptor's OnLink() multiple times — once per incoming peer.
    ///
    /// Requires an externally-managed executor (e.g., from AsioPoller).
    class UdpServer : public ITransport
    {
    public:
        struct Options
        {
            boost::asio::any_io_executor executor;

            /// Local port to bind. 0 = OS-assigned.
            std::uint16_t localPort = 0;

            /// Maximum UDP datagram payload size.
            std::size_t maxDatagramSize = 1472;
        };

        explicit UdpServer(Options options);
        ~UdpServer() override;

        // ITransport
        std::shared_ptr<IConnector> Open(std::shared_ptr<ILinkAcceptor> acceptor) override;

        /// Returns the local port actually bound (useful when localPort=0).
        [[nodiscard]] std::uint16_t LocalPort() const noexcept { return _localPort; }

    private:
        Options _options;
        std::uint16_t _localPort = 0;

        struct ListenState;
        std::shared_ptr<ListenState> _listenState;
    };

    static_assert(TransportLike<UdpServer>);
}
