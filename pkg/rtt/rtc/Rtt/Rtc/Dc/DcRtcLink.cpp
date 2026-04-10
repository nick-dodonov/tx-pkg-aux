#if !defined(__EMSCRIPTEN__)
#include "DcRtcLink.h"

#include "Rtt/Rtc/ISigUser.h"

#include <nlohmann/json.hpp>
#include <rtc/rtc.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Rtt::Rtc
{
    using json = nlohmann::json;

    static std::string_view ToStringView(rtc::PeerConnection::GatheringState st)
    {
        using G = rtc::PeerConnection::GatheringState;
        switch (st) {
        case G::New:        return "New";
        case G::InProgress: return "InProgress";
        case G::Complete:   return "Complete";
        }
        return "?";
    }

    static std::string_view ToStringView(rtc::PeerConnection::State st)
    {
        using S = rtc::PeerConnection::State;
        switch (st) {
        case S::New:          return "New";
        case S::Connecting:   return "Connecting";
        case S::Connected:    return "Connected";
        case S::Disconnected: return "Disconnected";
        case S::Failed:       return "Failed";
        case S::Closed:       return "Closed";
        }
        return "?";
    }

    // ---------------------------------------------------------------------------
    // Phase 1: Create — sets up PeerConnection and wires signaling + logging callbacks
    // ---------------------------------------------------------------------------

    DcRtcLink::DcRtcLink(
        rtc::Configuration config,
        PeerId localId,
        PeerId remoteId,
        std::weak_ptr<ISigUser> sigUser,
        std::size_t maxMessageSize,
        Log::Logger logger
    )
        : pc(std::move(config))
        , _localId(localId)
        , _remoteId(remoteId)
        , _maxMessageSize(maxMessageSize)
        , _logger(logger)
    {
        pc.onLocalDescription([sigUser, remoteId, logger](const rtc::Description& desc) mutable {
            if (auto su = sigUser.lock()) {
                logger.Trace("sending {} to {}", desc.typeString(), remoteId.value);
                const json payload = {{"type", desc.typeString()}, {"description", std::string(desc)}};
                su->Send(remoteId, payload.dump());
            }
        });

        pc.onLocalCandidate([sigUser, remoteId, logger](const rtc::Candidate& cand) mutable {
            if (auto su = sigUser.lock()) {
                logger.Trace("sending ICE candidate to {}", remoteId.value);
                const json payload = {{"type", "candidate"}, {"candidate", std::string(cand)}, {"mid", cand.mid()}};
                su->Send(remoteId, payload.dump());
            }
        });

        pc.onGatheringStateChange([localId, remoteId, logger](rtc::PeerConnection::GatheringState st) mutable {
            logger.Trace("ICE gathering [{} -> {}]: {}", localId.value, remoteId.value, ToStringView(st));
        });

        // Pre-link state change: diagnostic logging only.
        // Attach() will override this with a handler that fires onClosed/onFailed.
        pc.onStateChange([localId, remoteId, logger](rtc::PeerConnection::State st) mutable {
            logger.Trace("PC state [{} -> {}]: {}", localId.value, remoteId.value, ToStringView(st));
        });
    }

    // ---------------------------------------------------------------------------
    // Phase 2: Attach — bind the open DataChannel and wire data/disconnect callbacks
    // ---------------------------------------------------------------------------

    void DcRtcLink::Attach(
        std::shared_ptr<rtc::DataChannel> dc,
        std::function<void()> onClosed,
        std::function<void()> onFailed
    )
    {
        _dc = std::move(dc);
        _logger.Debug("data channel open {} -> {}", _localId.value, _remoteId.value);

        auto wlink = std::weak_ptr<DcRtcLink>{std::static_pointer_cast<DcRtcLink>(shared_from_this())};
        assert(wlink.expired() == false && "shared_from_this() must be valid when calling Attach()");

        _dc->onMessage([wlink](rtc::message_variant data) {
            auto self = wlink.lock();
            if (!self || self->_disconnectRequested) {
                return;
            }
            if (!std::holds_alternative<rtc::binary>(data)) {
                return;
            }
            const auto& bin = std::get<rtc::binary>(data);
            if (self->_handler.onReceived) {
                self->_handler.onReceived(std::span<const std::byte>{bin.data(), bin.size()});
            }
        });

        _dc->onClosed([wlink]() {
            // Log the DC-level close. onDisconnected fires later from PC state change
            // to guarantee all libdatachannel internal threads (SCTP/DTLS/ICE) are done.
            if (auto self = wlink.lock()) {
                self->_logger.Debug("data channel closed {} -> {}", self->_localId.value, self->_remoteId.value);
            }
        });

        // Override the pre-link onStateChange with one that fires onClosed/onFailed
        // and notifies the link handler. PC reaching a terminal state is later than
        // DC close — ensures ICE/DTLS teardown is complete before calling back.
        pc.onStateChange([wlink, onClosed = std::move(onClosed), onFailed = std::move(onFailed)](rtc::PeerConnection::State st) mutable {
            using S = rtc::PeerConnection::State;
            if (auto self = wlink.lock()) {
                self->_logger.Trace("PC state [{} -> {}]: {}", self->_localId.value, self->_remoteId.value, ToStringView(st));
            }
            if (st == S::Closed || st == S::Disconnected) {
                if (onClosed) {
                    onClosed();
                }
            } else if (st == S::Failed) {
                if (onFailed) {
                    onFailed();
                }
            }
            if (st == S::Closed || st == S::Disconnected || st == S::Failed) {
                auto self = wlink.lock();
                if (!self || self->_closedFired.exchange(true)) {
                    return;
                }
                self->_logger.Debug("peer connection closed {} -> {}", self->_localId.value, self->_remoteId.value);
                if (self->_handler.onDisconnected) {
                    self->_handler.onDisconnected();
                }
            }
        });
    }

    // ---------------------------------------------------------------------------
    // ICE negotiation
    // ---------------------------------------------------------------------------

    void DcRtcLink::SetRemoteDescription(const std::string& sdp, rtc::Description::Type type, const PeerId& fromPeerId)
    {
        _logger.Trace("{} received from {}", rtc::Description::typeToString(type), fromPeerId.value); //TODO: typeToStringView
        pc.setRemoteDescription(rtc::Description(sdp, type));

        std::vector<std::pair<std::string, std::string>> pending;
        {
            std::lock_guard lock{_mutex};
            _remoteDescSet = true;
            pending = std::move(_pendingCandidates);
        }
        if (!pending.empty()) {
            _logger.Trace("flushing {} pending ICE candidates", pending.size());
            for (auto& [cand, mid] : pending) {
                pc.addRemoteCandidate(rtc::Candidate(std::move(cand), std::move(mid)));
            }
        }
    }

    void DcRtcLink::AddRemoteCandidate(std::string cand, std::string mid, const PeerId& fromPeerId)
    {
        {
            std::lock_guard lock{_mutex};
            if (_remoteDescSet) {
                _logger.Trace("ICE candidate from {} (direct)", fromPeerId.value);
            } else {
                _logger.Trace("ICE candidate from {} (buffered, no remote desc yet)", fromPeerId.value);
                _pendingCandidates.emplace_back(std::move(cand), std::move(mid));
                return;
            }
        }
        pc.addRemoteCandidate(rtc::Candidate(std::move(cand), std::move(mid)));
    }

    // ---------------------------------------------------------------------------
    // ILink
    // ---------------------------------------------------------------------------

    const PeerId& DcRtcLink::LocalId() const
    {
        return _localId;
    }

    const PeerId& DcRtcLink::RemoteId() const
    {
        return _remoteId;
    }

    void DcRtcLink::SetHandler(LinkHandler handler)
    {
        _handler = std::move(handler);
    }

    void DcRtcLink::Send(WriteCallback writer)
    {
        if (_disconnectRequested || !_dc || !_dc->isOpen()) {
            return;
        }

        std::vector<std::byte> buf(_maxMessageSize);
        const auto bytesWritten = writer(std::span<std::byte>{buf});
        if (bytesWritten == 0) {
            return;
        }

        //TODO: enable verbose logging later and make it configurable (too noisy for now), make summary instead
        //_logger.Trace("sending {} bytes {} -> {}", bytesWritten, _localId.value, _remoteId.value);

        rtc::binary payload(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(bytesWritten));
        _dc->send(std::move(payload));
    }

    void DcRtcLink::Disconnect()
    {
        if (_disconnectRequested.exchange(true)) {
            return;
        }

        _logger.Trace("disconnect {} -> {}", _localId.value, _remoteId.value);

        // Close the DataChannel and PeerConnection asynchronously.
        // onDisconnected will fire from pc->onStateChange on both sides uniformly.
        if (_dc) {
            _dc->close();
        }
        pc.close();
    }
}
#endif
