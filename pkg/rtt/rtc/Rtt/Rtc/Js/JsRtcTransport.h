#pragma once
#if __EMSCRIPTEN__
#include "Rtt/Rtc/RtcOptions.h"
#include "Rtt/Transport.h"

#include <memory>

namespace Rtt::Rtc
{
    /// WebRTC transport for WASM/Emscripten using the browser's native RTCPeerConnection.
    ///
    /// Uses EM_JS to call into browser JavaScript for all WebRTC operations.
    /// All C++ callbacks are exported via EMSCRIPTEN_KEEPALIVE.
    ///
    /// Only available on WASM platforms.
    /// Use RtcClient::MakeDefault() / RtcServer::MakeDefault() for platform-independent code.
    class JsRtcTransport
        : public ITransport
        , public IConnector
        , public std::enable_shared_from_this<JsRtcTransport>
    {
    public:
        using Options = RtcOptions;

        explicit JsRtcTransport(Options options);
        ~JsRtcTransport() override;

        // ITransport
        [[nodiscard]] std::shared_ptr<IConnector> Open(std::shared_ptr<ILinkAcceptor> acceptor) override;

        // IConnector
        void Connect(PeerId remoteId) override;

    private:
        Options _options;

        struct State;
        std::shared_ptr<State> _state;
    };

    static_assert(TransportLike<JsRtcTransport>);
}
#endif
