#pragma once
#include "Acceptor.h"
#include "PeerId.h"

#include <memory>

namespace Rtt
{
    /// Outbound connection initiator returned by ITransport::Open().
    ///
    /// Non-null only for transports that support outbound connections.
    /// The connector is valid for the lifetime of the transport that
    /// produced it — the transport holds no separate reference cycles.
    class IConnector
    {
    public:
        virtual ~IConnector() = default;

        /// Initiate an outbound connection to the given remote peer.
        /// The resulting link is delivered via the acceptor passed to Open().
        /// May be called multiple times to connect to different remote peers.
        virtual void Connect(PeerId remoteId) = 0;
    };

    /// Compile-time contract for transport-like types.
    template <typename T>
    concept TransportLike = requires(T& t, std::shared_ptr<ILinkAcceptor> acceptor) {
        { t.Open(acceptor) } -> std::convertible_to<std::shared_ptr<IConnector>>;
    };

    /// Runtime-polymorphic transport interface.
    ///
    /// A single-role entry point for obtaining real-time links.
    /// Each concrete transport represents exactly one role (client or
    /// server) and exactly one technology (UDP, WebRTC, overlay, …).
    /// Addressing and configuration are implementation-specific and
    /// should be provided through concrete constructors or setters.
    class ITransport
    {
    public:
        virtual ~ITransport() = default;

        /// Activate this transport and register the acceptor for incoming links.
        ///
        /// The acceptor's OnLink() will be called when a link is established
        /// (with a live link on success, or an Error on failure). For server
        /// transports it may fire for each incoming connection.
        ///
        /// Returns a non-null IConnector if this transport supports outbound
        /// connections; null for pure-server transports. The returned pointer
        /// is kept alive by the transport's own shared ownership — callers
        /// do not need to hold it to keep the connector alive, but holding it
        /// prevents the transport from being destroyed prematurely.
        virtual std::shared_ptr<IConnector> Open(std::shared_ptr<ILinkAcceptor> acceptor) = 0;
    };

    static_assert(TransportLike<ITransport>);
}
