#if !defined(__EMSCRIPTEN__)
#include "DcRtcLink.h"

#include "Log/Log.h"

#include <rtc/rtc.hpp>

#include <cstring>
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
                                              std::size_t maxMessageSize)
{
    auto link = std::shared_ptr<DcRtcLink>(
        new DcRtcLink(std::move(localId), std::move(remoteId),
                      std::move(pc), std::move(dc), maxMessageSize));

    auto wlink = std::weak_ptr<DcRtcLink>{link};

    link->_dc->onMessage([wlink](rtc::message_variant data) {
        auto self = wlink.lock();
        if (!self || self->_closed) {
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
        auto self = wlink.lock();
        if (!self || self->_closed.exchange(true)) {
            return;
        }
        Log::Debug("data channel closed {} -> {}", self->_localId.value, self->_remoteId.value);
        if (self->_handler.onDisconnected) {
            self->_handler.onDisconnected();
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
    if (_closed || !_dc || !_dc->isOpen()) {
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
    if (_closed.exchange(true)) {
        return;
    }

    Log::Trace("disconnect {} -> {}", _localId.value, _remoteId.value);

    if (_dc && _dc->isOpen()) {
        _dc->close();
    }
    if (_pc) {
        _pc->close();
    }
    if (_handler.onDisconnected) {
        _handler.onDisconnected();
    }
}

} // namespace Rtt::Rtc
#endif
