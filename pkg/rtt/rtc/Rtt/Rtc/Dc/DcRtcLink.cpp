#if !defined(__EMSCRIPTEN__)
#include "DcRtcLink.h"

#include "Log/Log.h"

#include <rtc/rtc.hpp>

#include <cstring>
#include <functional>
#include <memory>
#include <vector>

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// DcRtcLink — private constructor
// ---------------------------------------------------------------------------

DcRtcLink::DcRtcLink(PeerId localId,
                     PeerId remoteId,
                     std::shared_ptr<rtc::PeerConnection> pc,
                     std::shared_ptr<rtc::DataChannel> dc,
                     std::size_t maxMessageSize)
    : _localId(std::move(localId))
    , _remoteId(std::move(remoteId))
    , _pc(std::move(pc))
    , _dc(std::move(dc))
    , _maxMessageSize(maxMessageSize)
{}

// ---------------------------------------------------------------------------
// Create — wires DC callbacks and returns a live link
// ---------------------------------------------------------------------------

std::shared_ptr<DcRtcLink> DcRtcLink::Create(PeerId localId,
                                              PeerId remoteId,
                                              std::shared_ptr<rtc::PeerConnection> pc,
                                              std::shared_ptr<rtc::DataChannel> dc,
                                              std::size_t maxMessageSize,
                                              std::function<void()> onClosed,
                                              std::function<void()> onFailed)
{
    auto link = std::shared_ptr<DcRtcLink>(
        new DcRtcLink(std::move(localId), std::move(remoteId),
                      std::move(pc), std::move(dc), maxMessageSize));

    auto wlink = std::weak_ptr<DcRtcLink>{link};

    link->_dc->onMessage([wlink](rtc::message_variant data) {
        auto self = wlink.lock();
        if (!self || self->_disconnectRequested) {
            return;
        }
        if (!std::holds_alternative<rtc::binary>(data)) {
            return;
        }
        const auto& bin = std::get<rtc::binary>(data);
        if (self->_handler.onReceived) {
            self->_handler.onReceived(
                std::span<const std::byte>{bin.data(), bin.size()});
        }
    });

    link->_dc->onClosed([wlink]() {
        // Log the DC-level close. onDisconnected fires later from PC state change
        // to guarantee all libdatachannel internal threads (SCTP/DTLS/ICE) are done.
        auto self = wlink.lock();
        if (self) {
            Log::Debug("data channel closed {} -> {}",
                       self->_localId.value, self->_remoteId.value);
        }
    });

    // Fire onDisconnected when the PeerConnection itself reaches a terminal
    // state. This is later than DC close and ensures ICE/DTLS teardown is
    // complete — safe to create new PeerConnections for the next test.
    link->_pc->onStateChange(
        [wlink, onClosed = std::move(onClosed), onFailed = std::move(onFailed)]
        (rtc::PeerConnection::State st)
        {
            if (st == rtc::PeerConnection::State::Closed ||
                st == rtc::PeerConnection::State::Disconnected) {
                if (onClosed) { onClosed(); }
            } else if (st == rtc::PeerConnection::State::Failed) {
                if (onFailed) { onFailed(); }
            }
            if (st == rtc::PeerConnection::State::Closed ||
                st == rtc::PeerConnection::State::Disconnected ||
                st == rtc::PeerConnection::State::Failed) {
                auto self = wlink.lock();
                if (!self || self->_closedFired.exchange(true)) {
                    return;
                }
                Log::Debug("peer connection closed {} -> {}",
                           self->_localId.value, self->_remoteId.value);
                if (self->_handler.onDisconnected) {
                    self->_handler.onDisconnected();
                }
            }
        });

    return link;
}

// ---------------------------------------------------------------------------
// ILink
// ---------------------------------------------------------------------------

const PeerId& DcRtcLink::LocalId() const { return _localId; }
const PeerId& DcRtcLink::RemoteId() const { return _remoteId; }

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

    Log::Trace("send {} bytes {} -> {}", bytesWritten, _localId.value, _remoteId.value);

    rtc::binary payload(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(bytesWritten));
    _dc->send(std::move(payload));
}

void DcRtcLink::Disconnect()
{
    if (_disconnectRequested.exchange(true)) {
        return;
    }

    Log::Trace("disconnect {} -> {}", _localId.value, _remoteId.value);

    // Close the DataChannel and PeerConnection asynchronously.
    // onDisconnected will fire from _dc->onClosed() on both sides uniformly.
    if (_dc) {
        _dc->close();
    }
    if (_pc) {
        _pc->close();
    }
}

} // namespace Rtt::Rtc
#endif
