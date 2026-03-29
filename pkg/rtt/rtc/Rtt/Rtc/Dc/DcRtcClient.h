#pragma once
#if !defined(__EMSCRIPTEN__)
#include "Rtt/Rtc/RtcOptions.h"
#include "Rtt/Transport.h"

#include <cstddef>
#include <memory>

namespace Rtt::Rtc
{
    /// Client-side WebRTC transport (Offerer).
    ///
    /// Opens a DataChannel-based connection to a remote peer by using the
    /// provided ISigClient for WebRTC signaling (SDP offer/answer + ICE).
    ///
    /// Only available on host and droid platforms (not WASM).
    /// Use RtcClient::MakeDefault() for platform-independent code.
    ///
    /// Lifetime:
    ///   Keep the DcRtcClient instance alive until the link has been delivered
    ///   via the acceptor — it holds the in-progress WebRTC negotiation state.
    ///   After the link is established, the link itself is independent.
    class DcRtcClient: public ITransport
    {
    public:
        using Options = RtcOptions;

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
}
#endif
