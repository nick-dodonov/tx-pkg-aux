#pragma once
#include "Rtt/Rtc/ISigClient.h"
#include "Rtt/Rtc/SigHub.h"

#include <memory>

namespace Rtt::Rtc
{

/// In-process signaling client (ISigClient implementation).
///
/// Routes messages directly through a SigHub without any network involvement.
/// Multiple LocalSigClient instances can share a single SigHub, allowing
/// any combination of in-process peers to exchange signaling messages.
///
/// Join() completes synchronously: onJoined is called before Join() returns.
///
/// Usage:
/// @code
///   auto hub = std::make_shared<SigHub>();
///   LocalSigClient client{hub};
///
///   std::shared_ptr<ISigUser> user;
///   client.Join(
///       PeerId{"alice"},
///       [](SigMessage m) { /* handle message */ },
///       [&](SigJoinResult result) {
///           if (result) user = *result;
///       });
/// @endcode
class LocalSigClient : public ISigClient
{
public:
    explicit LocalSigClient(std::shared_ptr<SigHub> hub);

    /// Register a peer on the shared hub.
    ///
    /// onJoined is called synchronously — before Join() returns.
    /// The returned ISigUser unregisters from the hub when destroyed.
    void Join(PeerId localId,
              SigMessageHandler onMessage,
              SigJoinHandler onJoined) override;

private:
    std::shared_ptr<SigHub> _hub;
};

static_assert(SigClientLike<LocalSigClient>);

} // namespace Rtt::Rtc
