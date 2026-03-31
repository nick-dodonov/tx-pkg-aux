#pragma once
#if !defined(__EMSCRIPTEN__)
#include "Rtt/Rtc/RtcOptions.h"
#include "Rtt/Transport.h"

#include <cstddef>
#include <memory>

namespace Rtt::Rtc
{
    /// Unified WebRTC transport (libdatachannel) that can act as offerer, answerer, or both.
    ///
    /// Behavior is controlled by RtcOptions:
    ///   - options.remoteId non-empty            → offerer: initiates a DataChannel to remoteId after join.
    ///   - options.maxInboundConnections > 0     → answerer: accepts incoming offers up to the limit.
    ///   Both modes can be active simultaneously.
    ///
    /// Each instance logs under "DcRtc/{localId}" for easy per-instance filtering.
    ///
    /// Only available on host and droid platforms (not WASM).
    /// Use RtcClient::MakeDefault() / RtcServer::MakeDefault() for platform-independent code.
    class DcRtcTransport: public ITransport
    {
    public:
        using Options = RtcOptions;

        explicit DcRtcTransport(Options options);
        ~DcRtcTransport() override;

        // ITransport — may call acceptor.OnLink() multiple times (one per remote peer).
        void Open(std::shared_ptr<ILinkAcceptor> acceptor) override;

    private:
        Options _options;

        struct State;
        std::shared_ptr<State> _state;
    };

    static_assert(TransportLike<DcRtcTransport>);
}
#endif
