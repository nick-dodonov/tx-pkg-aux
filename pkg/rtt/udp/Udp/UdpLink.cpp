#include "UdpLink.h"

#include <boost/asio/buffer.hpp>
#include <cstring>
#include <utility>

namespace Rtt::Udp
{
    // -----------------------------------------------------------------------
    // Connected mode constructor
    // -----------------------------------------------------------------------

    UdpLink::UdpLink(boost::asio::ip::udp::socket socket,
                     PeerId localId,
                     PeerId remoteId,
                     std::size_t maxDatagramSize)
        : _ownedSocket(std::move(socket))
        , _localId(std::move(localId))
        , _remoteId(std::move(remoteId))
        , _maxDatagramSize(maxDatagramSize)
        , _recvBuf(maxDatagramSize)
    {}

    // -----------------------------------------------------------------------
    // Shared mode constructor
    // -----------------------------------------------------------------------

    UdpLink::UdpLink(std::shared_ptr<boost::asio::ip::udp::socket> sharedSocket,
                     boost::asio::ip::udp::endpoint remoteEndpoint,
                     PeerId localId,
                     PeerId remoteId,
                     std::size_t maxDatagramSize,
                     RemoveFromDispatch removeFromDispatch)
        : _sharedSocket(std::move(sharedSocket))
        , _remoteEndpoint(std::move(remoteEndpoint))
        , _localId(std::move(localId))
        , _remoteId(std::move(remoteId))
        , _maxDatagramSize(maxDatagramSize)
        , _removeFromDispatch(std::move(removeFromDispatch))
        , _recvBuf(maxDatagramSize)
    {}

    // -----------------------------------------------------------------------
    // ILink
    // -----------------------------------------------------------------------

    const PeerId& UdpLink::LocalId() const { return _localId; }
    const PeerId& UdpLink::RemoteId() const { return _remoteId; }

    void UdpLink::Send(WriteCallback writer)
    {
        if (_closed) { 
            return;
        }

        std::vector<std::byte> buf(_maxDatagramSize);
        auto bytesWritten = writer(std::span<std::byte>{buf});
        if (bytesWritten == 0) { 
            return;
        }

        buf.resize(bytesWritten);

        if (_ownedSocket) {
            // Connected mode: async_send on dedicated socket
            auto self = ILink::shared_from_this();
            _ownedSocket->async_send(
                boost::asio::buffer(buf.data(), buf.size()),
                [self, data = std::move(buf)](boost::system::error_code, std::size_t) {
                    // Fire-and-forget — data kept alive by capture
                });
        } else if (_sharedSocket) {
            // Shared mode: async_send_to on shared socket
            auto self = ILink::shared_from_this();
            auto ep = _remoteEndpoint;
            _sharedSocket->async_send_to(
                boost::asio::buffer(buf.data(), buf.size()),
                ep,
                [self, data = std::move(buf)](boost::system::error_code, std::size_t) {
                    // Fire-and-forget
                });
        }
    }

    void UdpLink::Disconnect()
    {
        if (_closed) { 
            return;
        }
        _closed = true;

        if (_ownedSocket && _ownedSocket->is_open()) {
            boost::system::error_code ec;
            auto _ = _ownedSocket->close(ec);
        }

        if (_removeFromDispatch) {
            _removeFromDispatch();
        }

        if (_handler.onDisconnected) {
            _handler.onDisconnected();
        }
    }

    // -----------------------------------------------------------------------
    // Receive
    // -----------------------------------------------------------------------

    void UdpLink::StartReceive(LinkHandler handler)
    {
        _handler = std::move(handler);
        if (_ownedSocket) {
            DoReceive();
        }
    }

    void UdpLink::SetHandler(LinkHandler handler)
    {
        _handler = std::move(handler);
    }

    void UdpLink::DeliverReceived(std::span<const std::byte> data) const
    {
        if (!_closed && _handler.onReceived) {
            _handler.onReceived(data);
        }
    }

    void UdpLink::DoReceive()
    {
        if (_closed || !_ownedSocket) {
            return;
        }

        auto self = std::static_pointer_cast<UdpLink>(ILink::shared_from_this());
        _ownedSocket->async_receive(
            boost::asio::buffer(_recvBuf.data(), _recvBuf.size()),
            [self](boost::system::error_code ec, std::size_t bytesReceived) {
                if (ec) {
                    if (!self->_closed) {
                        self->Disconnect();
                    }
                    return;
                }
                if (self->_handler.onReceived) {
                    self->_handler.onReceived(
                        std::span<const std::byte>{self->_recvBuf.data(), bytesReceived});
                }
                self->DoReceive();
            });
    }
}
