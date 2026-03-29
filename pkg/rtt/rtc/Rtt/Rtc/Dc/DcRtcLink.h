#pragma once
#if !defined(__EMSCRIPTEN__)
#include "Rtt/Handler.h"
#include "Rtt/Link.h"

#include <atomic>
#include <cstddef>
#include <memory>

namespace rtc
{
    class PeerConnection;
    class DataChannel;
}

namespace Rtt::Rtc
{

/// ILink implementation over a WebRTC DataChannel (libdatachannel).
///
/// Only available on host and droid platforms (not WASM).
///
/// Ownership model:
///   - Holds shared ownership of the underlying PeerConnection and DataChannel.
///   - DC message/close callbacks hold a weak_ptr to self to avoid cycles.
///
/// Use Create() to construct — it wires the DC callbacks before returning.
/// Call SetHandler() immediately after OnLink() delivers the link to set
/// the data and disconnect handlers.
class DcRtcLink : public ILink
{
public:
    /// Construct and wire a new link.
    ///
    /// @param localId       Local peer ID.
    /// @param remoteId      Remote peer ID.
    /// @param pc            Owning PeerConnection (kept alive for link lifetime).
    /// @param dc            Open DataChannel used for data transfer.
    /// @param maxMessageSize Maximum send buffer size in bytes.
    [[nodiscard]] static std::shared_ptr<DcRtcLink> Create(
        PeerId localId,
        PeerId remoteId,
        std::shared_ptr<rtc::PeerConnection> pc,
        std::shared_ptr<rtc::DataChannel> dc,
        std::size_t maxMessageSize = 65535,
        std::function<void()> onClosed = {},
        std::function<void()> onFailed = {});

    // ILink
    [[nodiscard]] const PeerId& LocalId() const override;
    [[nodiscard]] const PeerId& RemoteId() const override;
    void Send(WriteCallback writer) override;
    void Disconnect() override;

    /// Bind the data and disconnect handlers.
    ///
    /// Must be called before any data can arrive (i.e., synchronously
    /// inside the dc->onOpen callback, before the lambda returns).
    void SetHandler(LinkHandler handler);

private:
    DcRtcLink(PeerId localId,
              PeerId remoteId,
              std::shared_ptr<rtc::PeerConnection> pc,
              std::shared_ptr<rtc::DataChannel> dc,
              std::size_t maxMessageSize);

    PeerId _localId;
    PeerId _remoteId;
    std::shared_ptr<rtc::PeerConnection> _pc;
    std::shared_ptr<rtc::DataChannel> _dc;
    std::size_t _maxMessageSize;
    LinkHandler _handler;
    std::atomic<bool> _disconnectRequested{false};
    std::atomic<bool> _closedFired{false};
};

static_assert(LinkLike<DcRtcLink>);

} // namespace Rtt::Rtc
#endif
