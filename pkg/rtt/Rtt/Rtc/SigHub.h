#pragma once
#include "ISigClient.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Rtt::Rtc
{

/// Message routing hub for in-process signaling.
///
/// SigHub manages a registry of active peers (identified by PeerId) and
/// routes messages between them by dispatching to their registered handlers.
///
/// Ownership model:
///   - SigHub is shared via shared_ptr among all clients/servers that use it.
///   - Each registered peer is stored internally. The ISigUser returned to the
///     caller keeps the registration alive; dropping it without calling Leave()
///     is fire-and-forget (no OnLeft notification).
///
/// Thread-safety:
///   - Register() and Dispatch() may be called concurrently from any thread.
///   - Message handlers are invoked outside the internal mutex to allow
///     re-entrant calls (e.g., handler calls Send() which calls Dispatch()).
class SigHub : public std::enable_shared_from_this<SigHub>
{
public:
    SigHub() = default;

    /// Register a peer on this hub.
    ///
    /// Calls onJoined with the new ISigUser to obtain the ISigHandler for
    /// message and disconnect routing. The returned ISigUser keeps the
    /// registration alive.
    ///
    /// If a peer with the same ID already exists, the old registration is
    /// replaced; the previous ISigUser becomes a disconnected alias.
    ///
    /// @param localId  The peer ID to register under.
    /// @param onJoined Factory called synchronously with SigJoinResult to obtain
    ///                 a weak_ptr<ISigHandler> for message delivery.
    void Register(PeerId localId, SigJoinHandler onJoined);

    /// Remove a peer registration. Called by ISigUser::Leave().
    void Unregister(const PeerId& localId);

    /// Route a message from one peer to another.
    ///
    /// Looks up the target peer, then invokes its message handler outside
    /// the internal lock. Silently discards the message if the target is
    /// unknown or has already been unregistered.
    void Dispatch(const PeerId& from, const PeerId& to, std::string payload);

private:
    class HubUser;

    std::mutex _mutex;
    std::unordered_map<std::string, std::weak_ptr<HubUser>> _peers;
};

} // namespace Rtt::Rtc
