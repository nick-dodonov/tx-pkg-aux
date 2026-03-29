#pragma once
#if !defined(__EMSCRIPTEN__)
#include "Rtt/Rtc/RtcOptions.h"
#include "Rtt/Transport.h"

#include <cstddef>
#include <memory>

namespace Rtt::Rtc
{
    /// Server-side WebRTC transport (Answerer).
    ///
    /// Listens for incoming WebRTC connection requests from remote peers
    /// by using the provided ISigClient for WebRTC signaling.
    /// Delivers a new link to the acceptor for each incoming DataChannel.
    ///
    /// Only available on host and droid platforms (not WASM).
    /// Use RtcServer::MakeDefault() for platform-independent code.
    class DcRtcServer: public ITransport
    {
    public:
        using Options = RtcOptions;

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
}
#endif
