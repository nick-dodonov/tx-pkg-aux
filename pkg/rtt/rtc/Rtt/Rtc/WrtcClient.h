#pragma once
#include "Rtt/Rtc/ISigClient.h"
#include "Rtt/Transport.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Rtt::Rtc
{

/// Factory for creating a platform-appropriate WebRTC client transport.
///
/// On host/droid: returns DcRtcClient (libdatachannel).
/// On wasm:       returns JsRtcClient (browser RTCPeerConnection via JS — NYI).
///
/// The transport opens a DataChannel to the specified remote peer using the
/// provided ISigClient for WebRTC signaling (SDP + ICE exchange).
class WrtcClient
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

    /// Create the platform-default ITransport implementation (Offerer/client).
    [[nodiscard]] static std::shared_ptr<ITransport> MakeDefault(Options options);
};

} // namespace Rtt::Rtc
