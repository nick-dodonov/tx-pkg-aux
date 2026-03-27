#pragma once
#include "Rtt/Transport.h"
#include <boost/asio/any_io_executor.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Rtt::Udp
{
    /// UDP implementation of Rtt::ITransport.
    ///
    /// Models connectionless UDP as virtual peer-to-peer links.
    /// - Connect() resolves a remote host:port, creates a connected UDP
    ///   socket, and delivers a link via the acceptor.
    /// - Listen() binds a local port and creates a new link for each
    ///   unique remote endpoint that sends a datagram.
    ///
    /// Requires an externally-managed executor (e.g., from AsioPoller).
    class UdpTransport : public ITransport
    {
    public:
        struct Options
        {
            boost::asio::any_io_executor executor;

            /// Remote host to connect to (Connect mode).
            std::string remoteHost;

            /// Remote port to connect to (Connect mode).
            std::uint16_t remotePort = 0;

            /// Local port to bind (Listen mode). 0 = OS-assigned.
            std::uint16_t localPort = 0;

            /// Maximum UDP datagram payload size. Used as the buffer size
            /// for Send() writer callbacks and receive buffers.
            std::size_t maxDatagramSize = 1472;
        };

        explicit UdpTransport(Options options);
        ~UdpTransport() override;

        // ITransport
        void Connect(std::shared_ptr<ILinkAcceptor> acceptor) override;
        void Listen(std::shared_ptr<ILinkAcceptor> acceptor) override;

        /// Returns the local port actually bound (useful when localPort=0).
        [[nodiscard]] std::uint16_t LocalPort() const noexcept { return _localPort; }

    private:
        Options _options;
        std::uint16_t _localPort = 0;

        struct ListenState;
        std::shared_ptr<ListenState> _listenState;
    };

    static_assert(TransportLike<UdpTransport>);
}
