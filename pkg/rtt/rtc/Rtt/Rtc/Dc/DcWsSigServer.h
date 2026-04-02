#pragma once
#if !defined(__EMSCRIPTEN__)
#include "Rtt/Rtc/SigHub.h"

#include <cstdint>
#include <memory>

namespace Rtt::Rtc
{

    /// WebSocket-based signaling server backed by libdatachannel.
    ///
    /// Only available on host and droid platforms (not WASM).
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
    /// @note DcWsSigServer is NOT copyable or movable after Start() has been called.
    class DcWsSigServer
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
        DcWsSigServer(std::shared_ptr<SigHub> hub, Options options);
        ~DcWsSigServer();

        DcWsSigServer(const DcWsSigServer&) = delete;
        DcWsSigServer& operator=(const DcWsSigServer&) = delete;
        DcWsSigServer(DcWsSigServer&&) = delete;
        DcWsSigServer& operator=(DcWsSigServer&&) = delete;

        /// Start accepting connections.  May be called at most once.
        void Start();

        /// Force-close one connected client's WebSocket.
        /// Triggers onClosed on both sides: the server will Leave() the hub user,
        /// and the client will receive OnLeft(SigError::ConnectionLost).
        /// No-op if the peer is not currently connected.
        void DisconnectPeer(const PeerId& peerId);

        /// The port the server is actually listening on.
        /// Returns 0 before Start() is called.
        [[nodiscard]] std::uint16_t Port() const noexcept;

    private:
        std::shared_ptr<SigHub> _hub;
        Options _options;

        struct Impl;
        std::shared_ptr<Impl> _impl; // shared_ptr so callbacks can safely weak_ptr to it
    };
}
#endif
