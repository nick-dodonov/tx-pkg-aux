#pragma once
#include "ISigUser.h"
#include "SigMessage.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Rtt::Rtc
{

/// Message routing hub for in-process and WebSocket signaling.
///
/// SigHub manages a registry of active peers (identified by PeerId) and
/// routes messages between them by dispatching to their registered handlers.
///
/// Ownership model:
///   - SigHub is shared via shared_ptr among all clients/servers that use it.
///   - Each registered peer is represented as an ISigUser whose shared_ptr the
///     caller must keep alive. When the caller drops the shared_ptr the peer is
///     automatically removed from the hub on the next Dispatch to that peer.
///
/// Thread-safety:
///   - Register() and Dispatch() may be called concurrently from any thread.
///   - Message handlers are invoked outside the internal mutex to allow
///     re-entrant calls (e.g., handler calls Send() which calls Dispatch()).
class SigHub : public std::enable_shared_from_this<SigHub>
{
public:
    SigHub() = default;

    /// Register a peer on this hub and return its user handle.
    ///
    /// If a peer with the same ID already exists, the old registration is
    /// replaced (the previous user handle becomes a stale alias).
    ///
    /// @param localId   The peer ID to register under.
    /// @param onMessage Called whenever a message is dispatched to this peer.
    [[nodiscard]] std::shared_ptr<ISigUser> Register(
        PeerId localId,
        SigMessageHandler onMessage);

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
