#pragma once
#include "Rtt/PeerId.h"
#include "Rtt/Error.h"
#include "ISigUser.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <system_error>

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// SigError — signaling-layer error codes
// ---------------------------------------------------------------------------

/// Error codes specific to the signaling layer.
enum class SigError
{
    ConnectionLost = 1, ///< The connection was dropped externally (server/network).
};

inline const std::error_category& sigErrorCategory() noexcept
{
    struct Cat : std::error_category
    {
        const char* name() const noexcept override { return "sig"; }
        std::string message(int ev) const override
        {
            switch (static_cast<SigError>(ev))
            {
                case SigError::ConnectionLost: return "connection lost";
            }
            return "unknown";
        }
    };
    static Cat cat;
    return cat;
}

inline std::error_code make_error_code(SigError e) noexcept
{
    return {static_cast<int>(e), sigErrorCategory()};
}

} // namespace Rtt::Rtc

template <>
struct std::is_error_code_enum<Rtt::Rtc::SigError> : std::true_type {};

namespace Rtt::Rtc
{
// ---------------------------------------------------------------------------
// SigMessage
// ---------------------------------------------------------------------------

/// A signaling message delivered to a registered user.
/// The payload is opaque to the signaling layer — it may carry SDP, ICE
/// candidates, or any other application-defined text.
struct SigMessage
{
    PeerId from;          ///< Sender's peer ID (set by the routing layer).
    std::string payload;  ///< Opaque text payload; format is application-defined.
};

// ---------------------------------------------------------------------------
// ISigHandler
// ---------------------------------------------------------------------------

/// Handler for incoming messages and disconnects for a joined peer.
struct ISigHandler {
    virtual ~ISigHandler() = default;

    virtual void OnMessage(SigMessage&& msg) = 0;

    /// Called when the peer is disconnected from the signaling channel, either
    /// voluntarily or due to an error. After this callback the handler will not
    /// receive any more messages and the connection is considered closed.
    ///
    /// @param ec Empty error code means a clean disconnect (initiated by Leave()).
    ///           SigError::ConnectionLost means the connection was dropped externally.
    virtual void OnLeft(std::error_code ec) = 0;
};

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
/// It represents a factory for ISigHandler instances, used by Join() to bind the message 
/// and disconnect handlers for a joined peer.
/// Real connection holds a weak_ptr to the handler because it doesn't own it.
/// Join caller is responsible for keeping the handler alive until the connection is closed.
/// In case of nullptr or lost reference, the connection will be closed immediately after joining.
using SigJoinHandler = std::function<std::weak_ptr<ISigHandler>(SigJoinResult)>;

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
    /// @param onJoined  Called once when registration completes or fails.
    ///                  May fire synchronously or from another thread.
    virtual void Join(PeerId localId, SigJoinHandler onJoined) = 0;
};

// ---------------------------------------------------------------------------
// Concept
// ---------------------------------------------------------------------------

template <typename T>
concept SigClientLike = requires(T& t,
                                  PeerId id,
                                  SigJoinHandler onJoined) {
    { t.Join(std::move(id), std::move(onJoined)) };
};

static_assert(SigClientLike<ISigClient>);

// ---------------------------------------------------------------------------
// ISigServer concept — any type with Start() and Port()
// ---------------------------------------------------------------------------

template <typename T>
concept SigServerLike = requires(const T& ct, T& t) {
    { t.Start() };
    { ct.Port() } -> std::convertible_to<std::uint16_t>;
};

} // namespace Rtt::Rtc
