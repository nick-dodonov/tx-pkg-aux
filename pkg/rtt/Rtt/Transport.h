#pragma once
#include "Acceptor.h"
#include <memory>

namespace Rtt
{
    /// Compile-time contract for transport-like types.
    template <typename T>
    concept TransportLike = requires(T& t, std::shared_ptr<ILinkAcceptor> acceptor) {
        { t.Open(acceptor) };
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

        /// Activate this transport.
        ///
        /// The acceptor's OnLink() will be called when a link is
        /// established (with a live link on success, or an Error on
        /// failure). For client transports this fires once; for server
        /// transports it may fire for each incoming connection.
        virtual void Open(std::shared_ptr<ILinkAcceptor> acceptor) = 0;
    };

    static_assert(TransportLike<ITransport>);
}
