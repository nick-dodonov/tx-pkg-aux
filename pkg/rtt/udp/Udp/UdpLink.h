#pragma once
#include "Rtt/Handler.h"
#include "Rtt/Link.h"

#include <boost/asio/ip/udp.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace Rtt::Udp
{
    /// Callback invoked by UdpLink when it wants to remove itself from the
    /// listener's dispatch map. Only used for links spawned by Listen().
    using RemoveFromDispatch = std::function<void()>;

    /// ILink implementation over a UDP socket.
    ///
    /// Two modes of operation:
    /// - **Connected** (from Connect): owns a dedicated socket connected to
    ///   one remote endpoint. Send uses async_send, receive uses async_receive.
    /// - **Shared** (from Listen): uses a shared unconnected socket owned by
    ///   UdpTransport. Send uses async_send_to, receive is dispatched from
    ///   the transport's receive loop.
    class UdpLink: public ILink
    {
    public:
        /// Construct a connected-mode link (from Connect).
        UdpLink(boost::asio::ip::udp::socket socket,
                PeerId localId,
                PeerId remoteId,
                std::size_t maxDatagramSize);

        /// Construct a shared-mode link (from Listen).
        UdpLink(std::shared_ptr<boost::asio::ip::udp::socket> sharedSocket,
                boost::asio::ip::udp::endpoint remoteEndpoint,
                PeerId localId,
                PeerId remoteId,
                std::size_t maxDatagramSize,
                RemoveFromDispatch removeFromDispatch);

        // ILink
        [[nodiscard]] const PeerId& LocalId() const override;
        [[nodiscard]] const PeerId& RemoteId() const override;
        void Send(WriteCallback writer) override;
        void Disconnect() override;

        /// Start the async receive loop (connected mode only).
        /// Must be called after the LinkHandler has been set.
        void StartReceive(LinkHandler handler);

        /// Deliver data from the transport's shared receive loop (shared mode).
        void DeliverReceived(std::span<const std::byte> data) const;

        /// Set the handler for disconnect notifications (shared mode).
        void SetHandler(LinkHandler handler);

    private:
        void DoReceive();

        // Connected mode: owns socket directly
        std::optional<boost::asio::ip::udp::socket> _ownedSocket;

        // Shared mode: borrows shared socket + knows target endpoint
        std::shared_ptr<boost::asio::ip::udp::socket> _sharedSocket;
        boost::asio::ip::udp::endpoint _remoteEndpoint;

        PeerId _localId;
        PeerId _remoteId;
        std::size_t _maxDatagramSize;
        RemoveFromDispatch _removeFromDispatch;
        std::vector<std::byte> _recvBuf;
        LinkHandler _handler;

        bool _closed = false;
    };

    static_assert(LinkLike<UdpLink>);
}
