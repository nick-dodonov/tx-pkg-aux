#pragma once
#include "Rtt/PeerId.h"
#include "Rtt/Error.h"
#include "ISigUser.h"
#include "SigMessage.h"

#include <expected>

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// SigJoinResult
// ---------------------------------------------------------------------------

/// Result of a Join attempt: a live user handle on success, an error on failure.
using SigJoinResult = std::expected<std::shared_ptr<ISigUser>, Error>;

// ---------------------------------------------------------------------------
// SigJoinHandler
// ---------------------------------------------------------------------------

/// Called when a Join attempt completes (success or failure).
/// May be invoked synchronously (LocalSigClient) or asynchronously (WsSigClient).
using SigJoinHandler = std::function<void(SigJoinResult)>;

// ---------------------------------------------------------------------------
// ISigClient
// ---------------------------------------------------------------------------

/// Entry point for joining a signaling channel under a given peer ID.
///
/// A single ISigClient instance may be used concurrently by multiple callers,
/// each joining under a different peer ID.
///
/// @note The callback-based Join lets each implementation choose its own
/// threading/polling strategy without imposing std::future overhead.
class ISigClient
{
public:
    virtual ~ISigClient() = default;

    /// Register a peer on the signaling channel.
    ///
    /// @param localId   The peer ID to register under.
    /// @param onMessage Called each time a message is routed to this peer.
    /// @param onJoined  Called once when registration completes or fails.
    ///                  May fire synchronously or from another thread.
    virtual void Join(PeerId localId,
                      SigMessageHandler onMessage,
                      SigJoinHandler onJoined) = 0;
};

// ---------------------------------------------------------------------------
// Concept
// ---------------------------------------------------------------------------

template <typename T>
concept SigClientLike = requires(T& t,
                                  PeerId id,
                                  SigMessageHandler onMessage,
                                  SigJoinHandler onJoined) {
    { t.Join(std::move(id), std::move(onMessage), std::move(onJoined)) };
};

static_assert(SigClientLike<ISigClient>);

} // namespace Rtt::Rtc
