#include "UdpCommon.h"
#include "UdpLink.h"
#include "UdpServer.h"

#include "Log/Log.h"
#include <boost/asio/ip/udp.hpp>
#include <map>
#include <memory>
#include <utility>

namespace Rtt::Udp
{
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    // -----------------------------------------------------------------------
    // ListenState — shared state for the receive loop
    // -----------------------------------------------------------------------

    struct UdpServer::ListenState
    {
        std::shared_ptr<udp::socket> socket;
        std::shared_ptr<ILinkAcceptor> acceptor;
        std::size_t maxDatagramSize{};
        udp::endpoint senderEndpoint;
        std::vector<std::byte> recvBuf;
        std::map<udp::endpoint, std::shared_ptr<UdpLink>> links;
        PeerId localId;
        bool stopped = false;

        void StartReceive()
        {
            if (stopped) {
                return;
            }

            socket->async_receive_from(
                asio::buffer(recvBuf.data(), recvBuf.size()),
                senderEndpoint,
                [this, self = socket](boost::system::error_code ec, std::size_t bytesReceived) {
                    if (ec || stopped) {
                        return;
                    }
                    OnReceived(bytesReceived);
                    StartReceive();
                });
        }

        void OnReceived(std::size_t bytesReceived)
        {
            auto data = std::span<const std::byte>{recvBuf.data(), bytesReceived};
            auto it = links.find(senderEndpoint);

            if (it != links.end()) {
                it->second->DeliverReceived(data);
                return;
            }

            auto remoteId = EndpointToPeerId(senderEndpoint);
            auto remoteEp = senderEndpoint;

            Log::Trace("new peer {} on {}", remoteId.value, localId.value);

            auto link = std::make_shared<UdpLink>(
                socket, remoteEp, localId, remoteId,
                maxDatagramSize,
                [this, remoteEp]() { links.erase(remoteEp); });

            links[remoteEp] = link;

            auto handler = acceptor->OnLink(link);
            link->SetHandler(std::move(handler));

            link->DeliverReceived(data);
        }
    };

    // -----------------------------------------------------------------------
    // UdpServer
    // -----------------------------------------------------------------------

    UdpServer::UdpServer(Options options)
        : _options(std::move(options))
    {}

    UdpServer::~UdpServer()
    {
        if (_listenState) {
            Log::Trace("closing on port {}", _localPort);
            _listenState->stopped = true;
            if (_listenState->socket && _listenState->socket->is_open()) {
                boost::system::error_code ec;
                auto _ = _listenState->socket->close(ec);
            }
        }
    }

    std::shared_ptr<IConnector> UdpServer::Open(std::shared_ptr<ILinkAcceptor> acceptor)
    {
        auto executor = _options.executor;
        auto port = _options.localPort;
        auto maxDgSize = _options.maxDatagramSize;

        boost::system::error_code ec;
        auto socket = std::make_shared<udp::socket>(executor);

        auto _ = socket->open(udp::v4(), ec);
        if (ec) {
            Log::Trace("socket open failed — {}", ec.message());
            acceptor->OnLink(std::unexpected(MapAsioError(ec)));
            return nullptr;
        }

        _ = socket->set_option(asio::socket_base::reuse_address(true), ec);
        _ = socket->bind(udp::endpoint(udp::v4(), port), ec);
        if (ec) {
            Log::Trace("bind failed on port {} — {}", port, ec.message());
            acceptor->OnLink(std::unexpected(MapAsioError(ec)));
            return nullptr;
        }

        _localPort = socket->local_endpoint().port();

        Log::Trace("listening on port {}", _localPort);

        _listenState = std::make_shared<ListenState>();
        _listenState->socket = std::move(socket);
        _listenState->acceptor = std::move(acceptor);
        _listenState->maxDatagramSize = maxDgSize;
        _listenState->recvBuf.resize(maxDgSize);
        _listenState->localId = EndpointToPeerId(_listenState->socket->local_endpoint());

        _listenState->StartReceive();
        return nullptr;
    }
}
