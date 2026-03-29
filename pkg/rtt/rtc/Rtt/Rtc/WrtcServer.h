#pragma once
#include "Rtt/Rtc/ISigClient.h"
#include "Rtt/Transport.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Rtt::Rtc
{

/// Factory for creating a platform-appropriate WebRTC server transport.
///
/// On host/droid: returns DcRtcServer (libdatachannel).
/// On wasm:       returns JsRtcServer (browser RTCPeerConnection via JS — NYI).
///
/// The transport listens for incoming WebRTC connections under the given
/// peer ID on the signaling channel and delivers a link per connected peer.
class WrtcServer
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

    /// Create the platform-default ITransport implementation (Answerer/server).
    [[nodiscard]] static std::shared_ptr<ITransport> MakeDefault(Options options);
};

} // namespace Rtt::Rtc
