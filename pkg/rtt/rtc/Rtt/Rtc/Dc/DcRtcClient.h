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

/// Client-side WebRTC transport (Offerer).
///
/// Opens a DataChannel-based connection to a remote peer by using the
/// provided ISigClient for WebRTC signaling (SDP offer/answer + ICE).
///
/// Only available on host and droid platforms (not WASM).
/// Use WrtcClient::MakeDefault() for platform-independent code.
///
/// Lifetime:
///   Keep the DcRtcClient instance alive until the link has been delivered
///   via the acceptor — it holds the in-progress WebRTC negotiation state.
///   After the link is established, the link itself is independent.
class DcRtcClient : public ITransport
{
public:
    struct Options
    {
        /// Signaling client used to exchange SDP and ICE with the remote peer.
        std::shared_ptr<ISigClient> sigClient;

        /// Local peer ID registered on the signaling channel.
        PeerId localId;

        /// Remote peer ID to connect to.
        PeerId remoteId;

        /// ICE server URIs, e.g. "stun:stun.l.google.com:19302".
        /// Empty = no STUN (suitable for loopback / in-process tests).
        std::vector<std::string> iceServers;

        /// Maximum send buffer size in bytes.
        std::size_t maxMessageSize = 65535;
    };

    explicit DcRtcClient(Options options);
    ~DcRtcClient() override;

    // ITransport
    void Open(std::shared_ptr<ILinkAcceptor> acceptor) override;

private:
    Options _options;

    // Holds in-progress negotiation state until the DataChannel is open.
    // Also keeps the PC/DC alive for ICE routing after link establishment.
    struct State;
    std::shared_ptr<State> _state;
};

static_assert(TransportLike<DcRtcClient>);

} // namespace Rtt::Rtc
#endif
