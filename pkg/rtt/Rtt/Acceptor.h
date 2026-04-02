#pragma once
#include "Error.h"
#include "Handler.h"
#include "Link.h"

#include <concepts>
#include <expected>
#include <memory>

namespace Rtt
{
    /// Result delivered when a connection is established or fails.
    /// Used for both outbound (Connect) and inbound (Listen) flows.
    using LinkResult = std::expected<std::shared_ptr<ILink>, Error>;

    /// Compile-time contract for link-acceptor-like types.
    template <typename T>
    concept LinkAcceptorLike = requires(T& t, LinkResult result) {
        { t.OnLink(std::move(result)) } -> std::convertible_to<LinkHandler>;
    };

    /// Runtime-polymorphic link acceptor interface.
    ///
    /// Handles the arrival of new links — whether from an outbound
    /// client or inbound server transport. Returns a LinkHandler that
    /// the transport binds to the link for subsequent data delivery
    /// and disconnect notification.
    class ILinkAcceptor
    {
    public:
        virtual ~ILinkAcceptor() = default;

        /// Called when a link is established (or when the attempt fails).
        ///
        /// On success the result contains a live link; the returned
        /// LinkHandler is bound to that link by the transport.
        /// On failure the result contains an Error; the returned
        /// LinkHandler is ignored.
        virtual LinkHandler OnLink(LinkResult result) = 0;
    };

    static_assert(LinkAcceptorLike<ILinkAcceptor>);
}
