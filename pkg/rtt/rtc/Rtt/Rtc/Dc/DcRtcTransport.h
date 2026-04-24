#pragma once
#if !defined(__EMSCRIPTEN__)
#include "Rtt/Rtc/RtcOptions.h"
#include "Rtt/Transport.h"

#include <memory>

namespace Rtt::Rtc
{
    /// Unified WebRTC transport (libdatachannel) that can act as offerer, answerer, or both.
    ///
    /// Behavior is controlled at Open()/Connect() time:
    ///   - Open()          → activates the transport; returns an IConnector for outbound use.
    ///   - Connect(id)     → initiates an outbound DataChannel to the given remote peer.
    ///   - maxInboundConnections > 0 → also accepts incoming offers up to the limit.
    ///
    /// Each instance logs under "DcRtc/{localId}" for easy per-instance filtering.
    ///
    /// Only available on host and droid platforms (not WASM).
    /// Use RtcClient::MakeDefault() / RtcServer::MakeDefault() for platform-independent code.
    class DcRtcTransport
        : public ITransport
        , public IConnector
        , public std::enable_shared_from_this<DcRtcTransport>
    {
    public:
        using Options = RtcOptions;

        explicit DcRtcTransport(Options options);
        ~DcRtcTransport() override;

        // ITransport — activates signaling and starts accepting inbound connections.
        // Returns this transport as IConnector (for outbound use via Connect()).
        std::shared_ptr<IConnector> Open(std::shared_ptr<ILinkAcceptor> acceptor) override;

        // IConnector — initiates an outbound DataChannel to the given remote peer.
        // May be called multiple times with different remote IDs after Open().
        void Connect(PeerId remoteId) override;

    private:
        Options _options;

        struct State;
        std::shared_ptr<State> _state;
    };

    static_assert(TransportLike<DcRtcTransport>);
}
#endif
