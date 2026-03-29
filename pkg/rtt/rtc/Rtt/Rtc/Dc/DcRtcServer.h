#pragma once
#if !defined(__EMSCRIPTEN__)
#include "Rtt/Rtc/ISigClient.h"
#include "Rtt/Transport.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Rtt::Rtc
{

/// Server-side WebRTC transport (Answerer).
///
/// Listens for incoming WebRTC connection requests from remote peers
/// by using the provided ISigClient for WebRTC signaling.
/// Delivers a new link to the acceptor for each incoming DataChannel.
///
/// Only available on host and droid platforms (not WASM).
/// Use WrtcServer::MakeDefault() for platform-independent code.
class DcRtcServer : public ITransport
{
public:
    struct Options
    {
        /// Signaling client used to exchange SDP and ICE with remote peers.
        std::shared_ptr<ISigClient> sigClient;

        /// Local peer ID registered on the signaling channel.
        PeerId localId;

        /// ICE server URIs, e.g. "stun:stun.l.google.com:19302".
        /// Empty = no STUN (suitable for loopback / in-process tests).
        std::vector<std::string> iceServers;

        /// Maximum send buffer size in bytes.
        std::size_t maxMessageSize = 65535;
    };

    explicit DcRtcServer(Options options);
    ~DcRtcServer() override;

    // ITransport — may call acceptor.OnLink() multiple times (one per peer).
    void Open(std::shared_ptr<ILinkAcceptor> acceptor) override;

private:
    Options _options;

    struct State;
    std::shared_ptr<State> _state;
};

static_assert(TransportLike<DcRtcServer>);

} // namespace Rtt::Rtc
#endif
