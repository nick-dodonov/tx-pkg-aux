#pragma once
#include "Rtt/Transport.h"

#include <boost/asio/any_io_executor.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Rtt::Udp
{
    /// Client-side UDP transport.
    ///
    /// Resolves a remote host:port and creates a connected UDP socket.
    /// Open() delivers exactly one link to the acceptor on success,
    /// or an error on failure.
    ///
    /// Requires an externally-managed executor (e.g., from AsioPoller).
    class UdpClient : public ITransport
    {
    public:
        struct Options
        {
            boost::asio::any_io_executor executor;

            /// Remote host to connect to.
            std::string remoteHost;

            /// Remote port to connect to.
            std::uint16_t remotePort = 0;

            /// Maximum UDP datagram payload size.
            std::size_t maxDatagramSize = 1472;
        };

        explicit UdpClient(Options options);

        // ITransport
        std::shared_ptr<IConnector> Open(std::shared_ptr<ILinkAcceptor> acceptor) override;

    private:
        Options _options;
    };

    static_assert(TransportLike<UdpClient>);
}
