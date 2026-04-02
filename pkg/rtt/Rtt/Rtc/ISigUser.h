#pragma once
#include "Rtt/PeerId.h"

#include <concepts>
#include <string>

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// ISigUser
// ---------------------------------------------------------------------------

/// Handle representing a peer that has joined a signaling channel.
///
/// A peer remains registered until Leave() is called explicitly or the
/// connection is dropped externally. Dropping the shared_ptr without calling
/// Leave() performs a fire-and-forget close and will NOT trigger ISigHandler::OnLeft.
class ISigUser
{
public:
    virtual ~ISigUser() = default;

    /// The local peer ID this user was registered under.
    [[nodiscard]] virtual const PeerId& LocalId() const = 0;

    /// Send a signaling message to another peer.
    ///
    /// The payload is forwarded verbatim; the signaling layer does not
    /// inspect or modify it. Fire-and-forget — no delivery confirmation.
    virtual void Send(const PeerId& to, std::string payload) = 0;

    /// Initiate a graceful leave from the signaling channel.
    ///
    /// Implementations must guarantee that ISigHandler::OnLeft is called
    /// exactly once after Leave() — with an empty error code — once the
    /// peer is fully deregistered (e.g. the underlying socket is closed).
    /// After Leave() returns, no further messages will be delivered.
    virtual void Leave() = 0;
};

// ---------------------------------------------------------------------------
// Concept
// ---------------------------------------------------------------------------

template <typename T>
concept SigUserLike = requires(T& t, const PeerId& to, std::string payload) {
    { t.LocalId() } -> std::convertible_to<const PeerId&>;
    { t.Send(to, std::move(payload)) };
    { t.Leave() };
};

static_assert(SigUserLike<ISigUser>);

} // namespace Rtt::Rtc
