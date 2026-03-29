#pragma once
#include "Rtt/Rtc/ISigServer.h"
#include "Rtt/Rtc/SigHub.h"

#include <cstdint>
#include <memory>

namespace Rtt::Rtc
{

/// WebSocket-based signaling server (ISigServer implementation).
///
/// Accepts WebSocket connections from signaling clients.  Each client
/// registers under a peer ID derived from the URL path it connects on:
///   ws://<host>:<port>/<peerId>
///
/// Wire protocol (JSON text frames, compatible with the Node.js reference
/// server):
///   Send:    {"id": "<targetPeerId>", "payload": "<text>"}
///   Receive: {"id": "<senderPeerId>", "payload": "<text>"}
///
/// The server delegates message routing to a shared SigHub so that multiple
/// clients and server instances can share the same routing domain.
///
/// @note WsSigServer is NOT copyable or movable after Start() has been called.
class WsSigServer : public ISigServer
{
public:
    struct Options
    {
        /// Port to listen on.  0 = OS-assigned (use Port() after Start()).
        std::uint16_t port = 0;
    };

    /// Construct the server.  Does not start listening yet.
    /// @param hub     Shared routing hub; must outlive this server.
    /// @param options Server configuration.
    WsSigServer(std::shared_ptr<SigHub> hub, Options options);
    ~WsSigServer() override;

    WsSigServer(const WsSigServer&) = delete;
    WsSigServer& operator=(const WsSigServer&) = delete;
    WsSigServer(WsSigServer&&) = delete;
    WsSigServer& operator=(WsSigServer&&) = delete;

    /// Start accepting connections.  May be called at most once.
    void Start();

    /// The port the server is actually listening on.
    /// Returns 0 before Start() is called.
    [[nodiscard]] std::uint16_t Port() const noexcept override;

private:
    std::shared_ptr<SigHub> _hub;
    Options _options;

    struct Impl;
    std::shared_ptr<Impl> _impl; // shared_ptr so callbacks can safely weak_ptr to it
};

static_assert(SigServerLike<WsSigServer>);

} // namespace Rtt::Rtc
