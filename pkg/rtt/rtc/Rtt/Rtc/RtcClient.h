#pragma once
#include "RtcOptions.h"
#include "Rtt/Transport.h"

namespace Rtt::Rtc
{
    /// Factory for creating a platform-appropriate WebRTC client transport.
    ///
    /// On host/droid: returns DcRtcClient (libdatachannel).
    /// On wasm:       returns JsRtcClient (browser RTCPeerConnection via JS — NYI).
    ///
    /// The transport opens a DataChannel to the specified remote peer using the
    /// provided ISigClient for WebRTC signaling (SDP + ICE exchange).
    class RtcClient
    {
    public:
        using Options = RtcOptions;

        /// Create the platform-default ITransport implementation (Offerer/client).
        [[nodiscard]] static std::shared_ptr<ITransport> MakeDefault(Options options);
    };
}
