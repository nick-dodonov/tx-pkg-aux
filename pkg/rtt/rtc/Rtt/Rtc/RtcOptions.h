#pragma once
#include "Rtt/Rtc/ISigClient.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Rtt::Rtc
{
    /// Common options shared by all WebRtc transport implementations.
    struct RtcOptions
    {
        /// Signaling client used to exchange SDP and ICE with the remote peer.
        std::shared_ptr<ISigClient> sigClient;

        /// Local peer ID registered on the signaling channel.
        PeerId localId;

        /// Remote peer ID to connect to (only used by the offerer: RtcClient and its implementations).
        PeerId remoteId;

        /// ICE server URIs, e.g. "stun:stun.l.google.com:19302".
        /// Empty = no STUN (suitable for loopback / in-process tests).
        std::vector<std::string> iceServers;

        /// Maximum send buffer size in bytes.
        std::size_t maxMessageSize = 65535;
    };
}
