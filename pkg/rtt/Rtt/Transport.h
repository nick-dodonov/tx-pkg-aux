#pragma once
#include "Acceptor.h"
#include <memory>

namespace Rtt
{
    /// Compile-time contract for transport-like types.
    template <typename T>
    concept TransportLike = requires(T& t, std::shared_ptr<ILinkAcceptor> acceptor) {
        { t.Connect(acceptor) };
        { t.Listen(acceptor) };
    };

    /// Runtime-polymorphic transport interface.
    ///
    /// Entry point for establishing and accepting real-time connections.
    /// Implementations may wrap WebRTC, raw UDP, overlay networks, etc.
    /// Addressing and configuration are implementation-specific (URLs for
    /// WebSocket, host/port for UDP, complex structs for WebRTC, etc.)
    /// and should be provided through concrete constructors or setters.
    class ITransport
    {
    public:
        virtual ~ITransport() = default;

        /// Initiate an asynchronous outbound connection.
        ///
        /// The acceptor's OnLink() will be called when the attempt
        /// completes (with a live link on success, or an Error on failure).
        virtual void Connect(std::shared_ptr<ILinkAcceptor> acceptor) = 0;

        /// Start listening for inbound connections.
        ///
        /// The acceptor's OnLink() will be called for each incoming
        /// connection (or once with an Error if listening fails).
        virtual void Listen(std::shared_ptr<ILinkAcceptor> acceptor) = 0;
    };

    static_assert(TransportLike<ITransport>);
}
