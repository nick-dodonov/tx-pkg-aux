#pragma once
#include "RtcOptions.h"
#include "Rtt/Transport.h"

namespace Rtt::Rtc
{
    /// Factory for creating a platform-appropriate WebRTC server transport.
    ///
    /// On host/droid: returns DcRtcServer (libdatachannel).
    /// On wasm:       returns JsRtcServer (browser RTCPeerConnection via JS — NYI).
    ///
    /// The transport listens for incoming WebRTC connections under the given
    /// peer ID on the signaling channel and delivers a link per connected peer.
    class RtcServer
    {
    public:
        using Options = RtcOptions;

        /// Create the platform-default ITransport implementation (Answerer/server).
        [[nodiscard]] static std::shared_ptr<ITransport> MakeDefault(Options options);
    };
}
