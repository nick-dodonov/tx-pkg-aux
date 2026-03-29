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
/// Dropping the shared_ptr automatically unregisters the peer from the
/// signaling hub — no explicit Leave() call required.
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
};

// ---------------------------------------------------------------------------
// Concept
// ---------------------------------------------------------------------------

template <typename T>
concept SigUserLike = requires(T& t, const PeerId& to, std::string payload) {
    { t.LocalId() } -> std::convertible_to<const PeerId&>;
    { t.Send(to, std::move(payload)) };
};

static_assert(SigUserLike<ISigUser>);

} // namespace Rtt::Rtc
