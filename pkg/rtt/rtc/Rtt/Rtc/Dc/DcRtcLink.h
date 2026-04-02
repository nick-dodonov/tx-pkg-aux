#pragma once
#if !defined(__EMSCRIPTEN__)
#include "Log/Log.h"
#include "Rtt/Handler.h"
#include "Rtt/Link.h"

#include <rtc/peerconnection.hpp>
#include <rtc/configuration.hpp>
#include <rtc/description.hpp>

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace rtc
{
    class PeerConnection;
    class DataChannel;
}

namespace Rtt::Rtc
{
    class ISigUser;

    /// ILink implementation over a WebRTC DataChannel (libdatachannel).
    ///
    /// Only available on host and droid platforms (not WASM).
    ///
    /// Two-phase construction:
    ///   1. Create()  — sets up PeerConnection with ICE config, signaling callbacks,
    ///                  and ICE candidate buffering. The PC is accessible via `pc`
    ///                  for external setup (createDataChannel / onDataChannel).
    ///   2. Attach()  — binds the open DataChannel and wires data/disconnect callbacks.
    ///                  Called from dc->onOpen (client) or pc->onDataChannel (server).
    ///
    /// After Attach(), pass the link to ILinkAcceptor::OnLink(), then call SetHandler()
    /// synchronously before returning from the callback.
    class DcRtcLink: public ILink
    {
    public:
        /// Phase 1: Create a configured PeerConnection and wire signaling + logging callbacks.
        ///
        /// @param config         configuration (e.g. ICE server URIs like "stun:stun.l.google.com:19302", etc).
        /// @param localId        Local peer identity.
        /// @param remoteId       Remote peer identity.
        /// @param sigUser        Signaling user for forwarding local SDP/ICE to the remote peer.
        /// @param logger         Logger instance carrying the caller's area tag.
        /// @param maxMessageSize Maximum send buffer size in bytes.
        DcRtcLink(
            rtc::Configuration config,
            PeerId localId,
            PeerId remoteId,
            std::weak_ptr<ISigUser> sigUser,
            std::size_t maxMessageSize,
            Log::Logger logger
        );

        /// Phase 2: Bind the open DataChannel and finalize the link.
        ///
        /// Overrides the pre-link onStateChange handler set in Create() with one
        /// that fires onClosed/onFailed and notifies the link handler.
        ///
        /// @param dc        Open DataChannel to use for data transfer.
        /// @param onClosed  Called when the PeerConnection reaches Closed/Disconnected.
        /// @param onFailed  Called when the PeerConnection reaches Failed.
        void Attach(
            std::shared_ptr<rtc::DataChannel> dc,
            std::function<void()> onClosed = {},
            std::function<void()> onFailed = {}
        );

        /// Apply a remote SDP description and flush any buffered ICE candidates.
        void SetRemoteDescription(const std::string& sdp, rtc::Description::Type type, const PeerId& fromPeerId);

        /// Apply or buffer a remote ICE candidate depending on whether SetRemoteDescription has already been called.
        void AddRemoteCandidate(std::string cand, std::string mid, const PeerId& fromPeerId);

        // ILink
        [[nodiscard]] const PeerId& LocalId() const override;
        [[nodiscard]] const PeerId& RemoteId() const override;
        void Send(WriteCallback writer) override;
        void Disconnect() override;

        /// Bind the data and disconnect handlers.
        ///
        /// Must be called synchronously after OnLink() delivers the link — before
        /// the Attach callback returns — so no data can arrive before handlers are set.
        void SetHandler(LinkHandler handler);

        /// Underlying PeerConnection — accessible for external callback setup
        /// (createDataChannel on client, onDataChannel on server).
        rtc::PeerConnection pc;

    private:
        PeerId _localId;
        PeerId _remoteId;
        std::size_t _maxMessageSize;
        Log::Logger _logger;
        std::shared_ptr<rtc::DataChannel> _dc;
        LinkHandler _handler;
        std::atomic<bool> _disconnectRequested{false};
        std::atomic<bool> _closedFired{false};

        std::mutex _mutex;
        bool _remoteDescSet{false};
        std::vector<std::pair<std::string, std::string>> _pendingCandidates;
    };

    static_assert(LinkLike<DcRtcLink>);
}
#endif
